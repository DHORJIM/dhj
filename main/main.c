
/***********************************************************************************************************
 * Ejemplo del uso de una máquina de estados
 * Manejo de una luz temporizada mediante un pulsador
 ***********************************************************************************************************/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "fsm.h"
#include "driver/i2c.h"

#define I2C_SCL_PIN 22
#define I2C_SDA_PIN 21
#define I2C_FREQ_HZ 100000
#define LCD_ADDR    0x27

/* Etiqueta para depuración */
const char* TAG = "app_main";
const char* TAG_VACIADO = "vaciado";
const char* TAG_DEP_VACIO = "deposito_vacio";
const char* TAG_LLENADO = "llenado";
const char* TAG_DEPOSITO_LLENO = "deposito lleno";
const char* TAG_MEDIDA_PUNTUAL = "medida puntual";
const char* TAG_MEDIDA_CONTINUADA = "medida_continuada";
const char* TAG_NO_LLENO_NO_VACIO = "deposito ni lleno ni vacio";

// Comandos del display LCD
#define LCD_CMD_CLEAR           0x01
#define LCD_CMD_RETURN_HOME     0x02
#define LCD_CMD_ENTRY_MODE_SET  0x04
#define LCD_CMD_DISPLAY_CONTROL 0x08
#define LCD_CMD_FUNCTION_SET    0x20
#define LCD_CMD_SET_CGRAM_ADDR  0x40
#define LCD_CMD_SET_DDRAM_ADDR  0x80

/* Pines GPIO asignados a la entrada (PULSADOR) y a la salida (LED) */
#define GPIO_PIN_LED    25 // Pin asignado al LED en el estado de emergencia
#define GPIO_PIN_BUTTON 26 // Pin asignado al pulsador de emergencia
#define GPIO_PIN_BUTTON_MEDIDA 32 // Pin asignado al pulsador de parada de medida

/* Estado de activación del pulsador (activo a nivel bajo) */
#define BUTTON_LEVEL_ACTIVE 0

/*Modos manual y automaticos que definiremos como booleana*/
#define LEVEL_MODO_MANUAL 0
#define LEVEL_MODO_AUTOMATICO 1

/*Estados de activacion y desactivacion de pesaje*/
#define PESAJE_LEVEL_INACTIVE 0
#define PESAJE_LEVEL_ACTIVE 1

/*Estados de activacion y desactivacion de pesaje*/
#define LEVEL_MEDIDA_PUNTUAL 0
#define LEVEL_MEDIDA_CONTINUADA 1

/*Niveles maximo y minimo de combustible*/
#define NIVEL_MAXIMO 10
#define NIVEL_MINIMO 0

/*Semaforo de gestion de los recursos compartidos*/
SemaphoreHandle_t mutex = NULL;


/* Periódo de revisión del estado de la máquina de estados */
#define FSM_CYCLE_PERIOD_MS 200
// #define FSM_CYCLE_PERIOD_MS 500

/* Tiempo que está activa la salida tras la pulsación del pulsador */
#define TIMER_CYCLE_S 4
#define TIMER_ESTABILIZADOR_S 5
#define TIMER_MEDIDA_S 1

/*Creamos esta variable para evitar que el valor de la medida al llenar o vaciar cree un bucle infinito*/
float medida_aux; 


// Estructura para intercambio de información con una tarea
typedef struct
{
    void* pConfig;  // Configuración de la tarea
    void* pDatos;   // Información para el proceso de la tarea
} taskInfo_t;

// Estructura para la info que se necesita para configurar la tarea
typedef struct
{
    uint32_t periodo;         // Periodo entre activaciones
} taskConfig_t;

/*Estructura para manejar la medida*/
typedef struct
{
    float nivel; /*Nivel de la medida*/
    SemaphoreHandle_t sem;    // Semáforo que gestiona la exclusión mutua de la medida
} MedidaInfo_t;



/* Cuenta del tiempo que para medidas y para completar la estabilizacion */
int timer_medida = 0;
int timer_estabilizador = 0;

/*Entrada digital que determina el pesaje activo o inactivo*/
int pesaje;


static void i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

static void lcd_send_command(uint8_t command) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LCD_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, command, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

static void lcd_send_data(uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LCD_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x40, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

static void lcd_init() {
    vTaskDelay(pdMS_TO_TICKS(50));
    lcd_send_command(LCD_CMD_FUNCTION_SET | 0x30);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_command(LCD_CMD_FUNCTION_SET | 0x30);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_send_command(LCD_CMD_FUNCTION_SET | 0x30);
    vTaskDelay(pdMS_TO_TICKS(1));

    lcd_send_command(LCD_CMD_FUNCTION_SET | 0x20);
    vTaskDelay(pdMS_TO_TICKS(1));

    lcd_send_command(LCD_CMD_FUNCTION_SET | 0x20);
    vTaskDelay(pdMS_TO_TICKS(1));

    lcd_send_command(LCD_CMD_DISPLAY_CONTROL | 0x08);
    vTaskDelay(pdMS_TO_TICKS(1));

    lcd_send_command(LCD_CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2));

    lcd_send_command(LCD_CMD_ENTRY_MODE_SET | 0x06);
    vTaskDelay(pdMS_TO_TICKS(1));
}

static void lcd_print(const char *str) {
    while (*str != '\0') {
        lcd_send_data(*str);
        str++;
    }
}

/* Comprueba si ha vencido el tiempo que debe estar activa la estabilizacion*/
int  timer_estabilizador_expired(void* params)
{
    return (timer_estabilizador == 0);
}

/* Comprueba si ha vencido el tiempo que debe estar activa la medida */
int  timer_medida_expired(void* params)
{
    return (timer_medida == 0);
    ESP_LOGD(TAG, "Se cumple periodo de medida");
}


/* Comienza el tiempo de medida activa */
void timer_estabilizador_start(void)
{
    timer_estabilizador = (TIMER_ESTABILIZADOR_S * 1000) / FSM_CYCLE_PERIOD_MS;
}


/* Comienza el tiempo de medida activa */
void timer_medida_start(void)
{
    timer_medida = (TIMER_MEDIDA_S * 1000) / FSM_CYCLE_PERIOD_MS;
}

/* Decrementa el tiempo que la salida debe permanecer activa */
void timer_estabilizador_next(void)
{
    if (timer_estabilizador) --timer_estabilizador;
    ESP_LOGD(TAG, "Tiempo restante de estabilizacion: %d", timer_estabilizador);

}
/* Decrementa el tiempo que la salida debe permanecer activa */
void timer_medida_next(void)
{
    if (timer_medida) --timer_medida;
}


/*Vaciamos el deposito*/
void vaciado (void* params){

    taskConfig_t* pConfig = ((taskInfo_t *)params)->pConfig;
    MedidaInfo_t*    pMedida    = ((taskInfo_t *)params)->pDatos;


    if (xSemaphoreTake(pMedida->sem, portMAX_DELAY)){
    while((pMedida->nivel)>(float)NIVEL_MINIMO){
         pMedida->nivel--; /*Vaciamos paulatinamente*/
    lcd_send_command(LCD_CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2));

    lcd_print("Vaciando deposito"); 
    }
    if((pMedida->nivel)<(float)NIVEL_MINIMO) pMedida->nivel = (float)NIVEL_MINIMO; /*Si queda por debajo de minimo, corregimos*/
    xSemaphoreGive(pMedida->sem);
    }
}
/*Comprobamos si el deposito esta vacio*/
int deposito_vacio(void* params){

    taskConfig_t* pConfig = ((taskInfo_t *)params)->pConfig;
    MedidaInfo_t*    pMedida    = ((taskInfo_t *)params)->pDatos;


    if (xSemaphoreTake(pMedida->sem, portMAX_DELAY)){
    medida_aux = pMedida->nivel;
    pMedida->nivel = pMedida->nivel+0.001;
    lcd_send_command(LCD_CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2));

    lcd_print("Deposito vacio"); 
        xSemaphoreGive(pMedida->sem);
    }
    return(medida_aux == (float)NIVEL_MINIMO);
}


/*Comprobamos si el deposito no esta ni lleno ni vacio*/
int deposito_no_lleno_no_vacio(void* params){

    taskConfig_t* pConfig = ((taskInfo_t *)params)->pConfig;
    MedidaInfo_t*    pMedida    = ((taskInfo_t *)params)->pDatos;


    if (xSemaphoreTake(pMedida->sem, portMAX_DELAY)){
    float medida_aux; /*Creamos esta variable para evitar que el valor de la medida al llenar o vaciar cree un bucle infinito*/
    medida_aux = pMedida->nivel;
        xSemaphoreGive(pMedida->sem);
    }
    return(medida_aux != (float)NIVEL_MINIMO && medida_aux != (float)NIVEL_MAXIMO);
}





/*Llenamos el deposito*/
void llenado (void* params){

    taskConfig_t* pConfig = ((taskInfo_t *)params)->pConfig;
    MedidaInfo_t*    pMedida    = ((taskInfo_t *)params)->pDatos;

    if (xSemaphoreTake(pMedida->sem, portMAX_DELAY)){
    while(pMedida->nivel<(float)NIVEL_MAXIMO){

    pMedida->nivel++; /*Vaciamos paulatinamente*/
    lcd_send_command(LCD_CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2));

    lcd_print("Llenando deposito"); 
    if(pMedida->nivel>(float)NIVEL_MAXIMO) pMedida->nivel = (float)NIVEL_MAXIMO; /*Si queda por debajo de minimo, corregimos*/
    xSemaphoreGive(pMedida->sem);
    }
}
/*Comprobamos si el deposito esta lleno*/
int deposito_lleno(void* params){

    taskConfig_t* pConfig = ((taskInfo_t *)params)->pConfig;
    MedidaInfo_t*    pMedida    = ((taskInfo_t *)params)->pDatos;

     if (xSemaphoreTake(pMedida->sem, portMAX_DELAY)){
    medida_aux = pMedida->nivel;
    pMedida->nivel = pMedida->nivel-0.001;
    lcd_send_command(LCD_CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2));

    lcd_print("Deposito lleno"); 
    xSemaphoreGive(pMedida->sem);
    }
    return(medida_aux == (float)NIVEL_MAXIMO);
}



/* Comprueba si el pulsador de emergencia está pulsado */
int pulsador_emergencia_pulsado(void *params) 
{ 
    bool button_state = (BUTTON_LEVEL_ACTIVE == gpio_get_level(GPIO_PIN_BUTTON));
    ESP_LOGD(TAG, "Estado de pulsador: %d", gpio_get_level(GPIO_PIN_BUTTON));
    if (button_state) ESP_LOGI(TAG, "Pulsador emergencia pulsado");

    return button_state;
}

/* Comprueba si el pulsador de parada de medida está pulsado */
int pulsador_parada_medida(void *params) 
{ 
    bool button_state = (BUTTON_LEVEL_ACTIVE == gpio_get_level(GPIO_PIN_BUTTON_MEDIDA));
    ESP_LOGD(TAG, "Estado de pulsador: %d", gpio_get_level(GPIO_PIN_BUTTON_MEDIDA));
    if (button_state) ESP_LOGI(TAG, "Pulsador parada medida pulsado");

    return button_state;
}

void toma_medida_puntual(void* params){

    taskConfig_t* pConfig = ((taskInfo_t *)params)->pConfig;
    MedidaInfo_t*    pMedida    = ((taskInfo_t *)params)->pDatos;


    if (xSemaphoreTake(pMedida->sem, portMAX_DELAY)){
    /*Ultimo valor medido*/
    float valor_actual;
     // En este ejemplo, se simula una medida aleatoria entre -1 y 1
    valor_actual = ((float)rand() / RAND_MAX) * 2-1;
    /*Vemos si la medida actual es el valor maximo*/
    if(valor_actual == (float)NIVEL_MAXIMO) {
        pMedida->nivel = (float)NIVEL_MAXIMO;}

    else {
        pMedida->nivel = (pMedida->nivel + valor_actual)/2.0;}

    lcd_send_command(LCD_CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2));

    lcd_print("Medida = "); /*Mostramos el valor de la medida por el display*/
    lcd_print((int)pMedida->nivel);
    xSemaphoreGive(pMedida->sem);
    }  
}

void toma_medida_continuada(void* params){

    taskConfig_t* pConfig = ((taskInfo_t *)params)->pConfig;
    MedidaInfo_t*    pMedida    = ((taskInfo_t *)params)->pDatos;

    if (xSemaphoreTake(pMedida->sem, portMAX_DELAY)){
    /*Ultimo valor medido*/
    float valor_actual;
     // En este ejemplo, se simula una medida aleatoria entre -1 y 1
    valor_actual = ((float)rand() / RAND_MAX) * 2-1;
    /*Vemos si la medida actual es el valor maximo*/
    if(valor_actual == (float)NIVEL_MAXIMO){
        pMedida->nivel = (float)NIVEL_MAXIMO;}

    else{
         pMedida->nivel = (pMedida->nivel + valor_actual)/2.0;}

    lcd_send_command(LCD_CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2));

    lcd_print("Medida ="); /*Mostramos el valor de la medida por el display*/
    lcd_print((int)pMedida->nivel);
    xSemaphoreGive(pMedida->sem);
    }
}

/* Activa el LED de salida y comienza su temporización */
void led_estabilizacion_on (void* params)
{
    pesaje = PESAJE_LEVEL_INACTIVE; /*Deshabilita pesaje*/
    ESP_LOGD(TAG, "Activa LED y comienza temporización para la estabilizacion");
    gpio_set_level(GPIO_PIN_LED, 1);
    timer_estabilizador_start();
}

/* Desactiva el LED de salida */
void led_estabilizacion_off (void* fsm)
{
    gpio_set_level(GPIO_PIN_LED, 0); /*Finaliza la estabilizacion y se desactiva el led*/
    ESP_LOGD(TAG, "Estabilizacion terminada");
    pesaje = PESAJE_LEVEL_ACTIVE; /*Habilitamos pesaje*/
}

int pesaje_activo(){
    return pesaje ; /*Comprobamos si el pesaje se encuentra activo*/
}


/* Prepara la máquina de estados que controla la luz temporizada */
fsm_t* modo_puntual_new (void)
{
    static fsm_trans_t puntual_tt[] = {
        {  0, pesaje_activo, 1, timer_medida_start},
        {  1, timer_medida_expired, 2, toma_medida_puntual },      
        {  1, pulsador_emergencia_pulsado, 5, led_estabilizacion_on},
        {  2, deposito_no_lleno_no_vacio, -1, NULL},  
        {  2, deposito_lleno, 3, vaciado},  
        {  2, deposito_vacio, 4, llenado},
        {  2, pulsador_emergencia_pulsado, 5, led_estabilizacion_on},
        {  3, deposito_vacio, -1, NULL},  
        {  3, pulsador_emergencia_pulsado, 5, led_estabilizacion_on},
        {  4, deposito_lleno, -1, NULL},
        {  4, pulsador_emergencia_pulsado, 5, led_estabilizacion_on},
        {  5, timer_estabilizador_expired, -1, led_estabilizacion_off},
        { -1, NULL, -1, NULL },
    };
    return fsm_new (puntual_tt);
}


/*Prepara la maquina para la medida en modo manual*/
fsm_t* modo_continuado_new (void)
{
    static fsm_trans_t continuado_tt[] = {
        {  0, pesaje_activo, 1, timer_medida_start },
        {  1, timer_medida_expired, 7, toma_medida_continuada},
        {  7, 1, 1, timer_medida_start},
        {  1, deposito_lleno, 2, vaciado},
        {  1, deposito_vacio, 3, llenado},
        {  1, pulsador_emergencia_pulsado, 5, led_estabilizacion_on},
        {  1, pulsador_parada_medida, 6, NULL},
        {  2, deposito_vacio, 0, NULL},
        {  2, pulsador_emergencia_pulsado, 5, led_estabilizacion_on},
        {  3, deposito_lleno, 0, NULL},
        {  3, pulsador_emergencia_pulsado, 5, led_estabilizacion_on},
        {  5, timer_estabilizador_expired, -1, led_estabilizacion_off},
        {  6, timer_medida_expired, -1, toma_medida_puntual},       
        { -1, NULL, -1, NULL },
    };
    return fsm_new (continuado_tt);
}



/* Tarea de gestión de la máquina de estados */
void app_main() 
{


    MedidaInfo_t medida;
    /*Inicializacion del maestro en el protocolo I2C, asi como del lcd*/
    i2c_master_init();
    lcd_init();

    pesaje = PESAJE_LEVEL_ACTIVE; /*Inicializamos el pesaje*/

    int comando; /*Numero entero como interprete de comando introducido por la consola*/
    /* Prepara la entrada y la salida */
    ESP_LOGI(TAG, "Preparando entrada y salida");
    gpio_reset_pin(GPIO_PIN_LED);
    gpio_reset_pin(GPIO_PIN_BUTTON);
    gpio_set_direction(GPIO_PIN_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_PIN_BUTTON, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_PIN_BUTTON, GPIO_PULLUP_ONLY);

     // Prepara el semáforo
    mutex = xSemaphoreCreateMutex(); 
    if (mutex == NULL)
    {
        ESP_LOGI(TAG, "Error al intentar crear el mutex");
        return;
    }

    // Información de configuración de cada tarea
    taskConfig_t vaciado_config, deposito_vacio_config, llenado_config, deposito_lleno_config, medida_puntual_config, medida_continuada_config;
    taskConfig_t no_lleno_no_vacio_config;
    // Estructura de paso de información a cada tarea
    taskInfo_t vaciado_info, deposito_vacio_info, llenado_info, deposito_lleno_info, medida_puntual_info, medida_continuada_info;
    taskInfo_t no_lleno_no_vacio_info;
   
    // Configuración de cada tarea
    vaciado_config.periodo = FSM_CYCLE_PERIOD_MS;
    deposito_vacio_config.periodo = FSM_CYCLE_PERIOD_MS;
    llenado_config.periodo = FSM_CYCLE_PERIOD_MS;
    deposito_lleno_config.periodo = FSM_CYCLE_PERIOD_MS;
    medida_puntual_config.periodo = FSM_CYCLE_PERIOD_MS;
    medida_continuada_config.periodo = FSM_CYCLE_PERIOD_MS;
    no_lleno_no_vacio_config.periodo = FSM_CYCLE_PERIOD_MS;

    // Información completa de cada tarea
    vaciado_info.pConfig = (void* )&vaciado_config;
    vaciado_info.pDatos  = (void* )&medida;

    deposito_vacio_info.pConfig = (void* )&deposito_vacio_config;
    deposito_vacio_info.pDatos  = (void* )&medida;

    llenado_info.pConfig = (void* )&llenado_config;
    llenado_info.pDatos  = (void* )&medida;    

    deposito_lleno_info.pConfig = (void* )&deposito_lleno_config;
    deposito_lleno_info.pDatos  = (void* )&medida;

    medida_puntual_info.pConfig = (void* )&medida_puntual_config;
    medida_puntual_info.pDatos  = (void* )&medida;

    medida_continuada_info.pConfig = (void* )&medida_continuada_config;
    medida_continuada_info.pDatos  = (void* )&medida;

    no_lleno_no_vacio_info.pConfig = (void* )&no_lleno_no_vacio_config;
    no_lleno_no_vacio_info.pDatos  = (void* )&medida;

    /*Creamos las tareas que necesitaran del semaforo para ejecutarse haciendo uso de los datos compartidos por exc.mutua*/
    TaskHandle_t t_vaciado, t_dep_vacio,t_llenado,t_dep_lleno,t_medida_puntual,t_medida_continuada,t_no_lleno_no_vacio; //Manejadores de las tareas
    xTaskCreate(vaciado,TAG_VACIADO,  2048, &vaciado_info, 4, &t_vaciado ); /*Creacion de la tarea de vaciado*/
    xTaskCreate(deposito_vacio, TAG_DEP_VACIO,  2048, &deposito_vacio_info, 4, &t_dep_vacio ); /*Creacion tarea deposito vacio*/
    xTaskCreate(llenado, TAG_LLENADO,  2048, &llenado_info, 4, &t_llenado ); /*Creacion de la tarea de llenado*/
    xTaskCreate(deposito_lleno, TAG_DEPOSITO_LLENO,  2048, &deposito_lleno_info, 4, &t_dep_lleno ); /*Creacion de la tarea de deposito lleno*/
    xTaskCreate(toma_medida_puntual, TAG_MEDIDA_PUNTUAL,  2048, &medida_puntual_info, 4, &t_medida_puntual); /*Creacion tarea de toma de medida puntual*/
    xTaskCreate(toma_medida_continuada, TAG_MEDIDA_CONTINUADA,  2048, &medida_continuada_info, 4, &t_medida_continuada); /*Creacion tarea de toma de medida continuada*/
    xTaskCreate(deposito_no_lleno_no_vacio  , TAG_NO_LLENO_NO_VACIO,  2048, &no_lleno_no_vacio_info, 4, &t_no_lleno_no_vacio); /*Creacion tarea de toma de medida continuada*/



    /* Configura la máquina de estados */
    ESP_LOGI(TAG, "Configura el comportamiento del sistema");
    fsm_t* fsm_puntual = modo_puntual_new();
    fsm_t* fsm_continuado = modo_continuado_new();

    /* Ciclo de gestión de la máquina */
    ESP_LOGI(TAG, "Pone a funcionar el sistema");
    TickType_t last = xTaskGetTickCount();

    while (true) {
        /*Pedimos modo de funcionamiento por consola de comandos*/
        printf("Introdce 0 para funcionamiento manual, o 1 para funcionamiento automatico: ");
        scanf("%d", &comando);
        if(comando == LEVEL_MODO_MANUAL){ /*Modo manual*/
        printf("Introdce 0 para medida puntual, o 1 para medida continuada: ");
        scanf("%d", &comando);
        if(comando == LEVEL_MEDIDA_PUNTUAL){ /*Modo puntual*/
        fsm_update (fsm_puntual);
        timer_medida_next();
        timer_estabilizador_next();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(FSM_CYCLE_PERIOD_MS));
        }
        if(comando == LEVEL_MEDIDA_CONTINUADA){ /*Modo continuado*/
        fsm_update (fsm_continuado);
        timer_medida_next();
        timer_estabilizador_next();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(FSM_CYCLE_PERIOD_MS));
        }
        }
       if(comando == LEVEL_MODO_AUTOMATICO) {/*Si el modod es automatico, siempre se mide en modo continuado*/
        fsm_update (fsm_continuado);
        timer_medida_next();
        timer_estabilizador_next();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(FSM_CYCLE_PERIOD_MS));
    }
 }
}
