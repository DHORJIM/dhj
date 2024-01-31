// Interfaz pública de manejo de una máquina de estados finitos
// (Fnite State Machine)

#ifndef FSM_H
#define FSM_H

// Tipo (adelantado) de una máquina de estados
typedef struct fsm fsm_t;

// Tipo (adelantado) de una transición en una máquina de estados
typedef struct fsm_trans fsm_trans_t;

// Tipos de las transiciones y salidas de cada estado
// Condición de transición
typedef int  (*fsm_input_func_t)  (void*);
// Salida
typedef void (*fsm_output_func_t) (void*);

// Estructura con una máquina de estados finitos
struct fsm {
  int current_state;  // Estado actual
  fsm_trans_t* tt;    // Tabla de transiciones
};

// Estructura con cada una de las tansiciones de la máquina
struct fsm_trans {
  int orig_state;         // Estado de partida
  fsm_input_func_t in;    // Condición de transición
  int dest_state;         // Estado de llegada
  fsm_output_func_t out;  // Función de salida
};

// Funciones públicas

// Crea una nueva máquina de estados
fsm_t* fsm_new (fsm_trans_t* tt);

// Inicializa una máquina de estados con una tabla de transiciones
void fsm_init (fsm_t* self, fsm_trans_t* tt);

// Actualiza una máquina de estados
void fsm_update (fsm_t* self);

#endif
