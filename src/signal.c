#include <signal.h>
#include <stdio.h>

#include "signal.h"
#include "mempartagee.h"

static volatile sig_atomic_t sigintRecu = 0;


static void GestionnaireSigint([[ maybe_unused ]] int sig) {
   sigintRecu = 1;
}

void RestaurerEtatSigint(void) {
   sigintRecu = 0;
}

bool VerifierSigintEnAttente(void) {
   return sigintRecu;
}

bool MiseEnPlaceGestionnairesSignaux(void) {
   struct sigaction action;

   action.sa_handler = GestionnaireSigint;
   sigemptyset(&action.sa_mask);
   action.sa_flags = 0;

   if (sigaction(SIGINT, &action, NULL) < 0) {
      perror("MiseEnPlaceGestionnairesSignaux::sigaction(SIGINT)");
      return 0;
   }

   action.sa_handler = GestionnaireSigusr1;
   if (sigaction(SIGUSR1, &action, NULL) < 0) {
      perror("MiseEnPlaceGestionnairesSignaux::sigaction(SIGUSR1)");
      return 0;
   }

   signal(SIGPIPE, SIG_IGN);
   return 1;
}

bool TraiterSigint(MessageSuspendu* messageSuspendu) {
   if (messageSuspendu != NULL && VerifierSigintEnAttente()) {
      FlushBuffer(messageSuspendu);
      RestaurerEtatSigint();
      return true;
   }

   return false;
}
