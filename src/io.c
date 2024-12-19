#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>

#include "io.h"
#include "mempartagee.h"
#include "signal.h"

static bool ReadNBytes(int fd, char* buffer, uint32_t n, MessageSuspendu* messageSuspendu) {
   for (uint32_t i = 0; i < n; ) {
      int32_t readCount = (int32_t)read(fd, &buffer[i], n - i);

      if (readCount == 0) {
         return false;
      } else if (readCount < 0) {
         if (errno != EINTR) {
            perror("ReadNBytes()");
            return false;
         } else if (errno == EINTR) {
            if (!TraiterSigint(messageSuspendu))
               return false;

            continue;
         }
      } else {
         i += (uint32_t)readCount;
      }

      if (VerifierSigintEnAttente() && !TraiterSigint(messageSuspendu))
         return false;
   }

   return true;
}


bool ReadStreamMessage(int fd, char* buffer, uint32_t maxSize, uint32_t* size, MessageSuspendu* messageSuspendu) {
   *size = 0;
   if (!ReadNBytes(fd, (char*)size, 4, messageSuspendu))
      return false;

   if (*size > maxSize) {
      fprintf(stderr, "Client tried to send more (%d) than %d octets.\n", *size, maxSize);
      return false;
   }

   if (!ReadNBytes(fd, buffer, *size, messageSuspendu))
      return false;

   return true;
}

static bool WriteNBytes(int fd, char* buffer, uint32_t n, MessageSuspendu* messageSuspendu) {
   for (uint32_t i = 0; i < n; ) {
      int32_t writeCount = (int32_t)write(fd, &buffer[i], n - i);

      if (writeCount < 0) {
         if (errno == EINTR) {
            if (!TraiterSigint(messageSuspendu))
               return false;

            continue;
         } else if (errno == EPIPE) {
            // Pas de message d'erreur nécessaire, le client s'est juste
            // déconnecté.
            return false;
         } else {
            perror("WriteNBytes()");
            return false;
         }
      } else {
         i += (uint32_t)writeCount;
      }

      if (VerifierSigintEnAttente() && !TraiterSigint(messageSuspendu))
         return false;
   }

   return true;
}

bool WriteStreamMessage(int fd, char* buffer, uint32_t size, MessageSuspendu* messageSuspendu) {
   if (!WriteNBytes(fd, (char*)&size, 4, messageSuspendu))
      return false;

   if (!WriteNBytes(fd, buffer, size, messageSuspendu))
      return false;

   return true;
}
