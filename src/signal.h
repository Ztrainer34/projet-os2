#ifndef SIGNAL_H__
#define SIGNAL_H__

#include <stdbool.h>

#include "mempartagee.h"

bool VerifierSigintEnAttente(void);
bool VerifierSigusr1EnAttente(void);

bool MiseEnPlaceGestionnairesSignaux(void);

bool TraiterSigint(MessageSuspendu* messageSuspendu);

#endif
