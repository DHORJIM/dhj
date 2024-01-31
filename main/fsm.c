// Realización del manejo de una máquina de estados finitos
// (Finite State Machine)

#include <stdlib.h>
#include "fsm.h"

/***** Crea una nueva máquina de estados *****/
fsm_t* fsm_new (fsm_trans_t* tt)
{
  fsm_t* p_fsm = (fsm_t*) malloc (sizeof (fsm_t)); // Reserva memoria papra la máquina
  fsm_init (p_fsm, tt);  // La inicializa con una tabla de transiciones
  return p_fsm;  // Devuelve la dirección de la máquina recien creada
}

/***** Inicializa una máquina de estados *****/
void fsm_init (fsm_t* p_fsm, fsm_trans_t* tt)
{
  p_fsm->tt = tt; // Conecta la máquina con una tabla de transiciones
  p_fsm->current_state = tt[0].orig_state; // El primer estado es el estado de partir de la primera transición de la tabla
}

/***** Actualiza una máquina de estados *****/
void fsm_update (fsm_t* p_fsm)
{
  fsm_trans_t* t;

// Recorre la tabla de transiciones hasta encontrar una con estado origen < 0
// (Marca de fin de tabla)
  for (t = p_fsm->tt; t->orig_state >= 0; ++t) {
    if ((p_fsm->current_state == t->orig_state) && t->in(p_fsm)) {
      // Si el estado actual de la máquina es el origen de la transición
      // que se está evaluando y se cumple la condición de transición
      // Actualiza el estado actual de la máquina al destino de la transición
      p_fsm->current_state = t->dest_state;

      // Y si hay salida se actualiza la salida
      if (t->out)
        t->out(p_fsm);

      // Si se produce una transición se deja de evaluar la tabla  
      break;
    }
  }
}
