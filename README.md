| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- |

# _main

En este proyecto, se hara una implementacion de un medidor gavimetrico de consumo de combustible.

Una vez iniciado el programa, se habilitará el pesaje de manera automatica, de manera que se solicitará al usuario determinar el modo de funcionamiento a traves de la introduccion de un numero por la consola de comando. En funcion de si la eleccion de modo es manual o automatica, se puede solicitar la introduccion de un segundo comando, ya que en caso de querer trabajar en modo manual se necesita determinar si se requiere una medida puntual o una serie de medidas continuadas.

En el caso de efectuar una medida puntual, el sistema devolvera una medida media en el periodo establecido, comparando la medida inicial y la última medida tomada. Si se detecta que tras la medida el deposito esta lleno, se determinará el proceso de vaciado al igual que el de llenado en caso de detectar que esta vacio.

En el caso de efectuar una medida continuada, dispondremos de un boton para determinar cuando ha de pararse la medida. De igual manera se han implementado los procesos de llenado y vaciado.

Para las medidas, se ha generado un algoritmo que generar un numero aleatorio entre -1 y 1 de manera que este sea la cantidad sumada a la medida del deposito actual, simulando el caudal de entrada o salida del sistema.

Se ha implementado de igual manera un pulsador de emergencia para poder hacer una parada segura del sistema en cualquier estado, pasando a un proceso de estabilizacion.

Para proteger las variables compartidas del proceso, que en este caso es la medida del sistema, se hara uso de exclusión mutua a traves de estructuras de datos en las que solamente una de las tareas que se estñe ejecutando será capaz de haceer uso de este recurso, que consta con su propio semáforo independiente del mutex de la tarea principal.


La medidas puntuales y continuadas han sido implementadas por maquinas de estados, las cuales vendran mas detalladas en el documento que se proporcionara de manera independiente a este archivo.

Cabe destacar, por último, que se ha hecho uso de un display cuyo uso vendrá dado a través del protocolo i2c.



#   d h j 
 
 