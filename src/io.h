#ifndef IO_H_
#define IO_H_

#include <stdbool.h>
#include <stdint.h>

#include "mempartagee.h"

/**
 * Lit un message sur un flux d'octets (un pipe nommé dans
 * notre implémentation) référencé par le descripteur de
 * fichiers `fd`. Le message est conservé dans `buffer`
 * et a sa taille conservée dans `*size` (cette dernière
 * ne pouvant dépasser `maxSize`).
 *
 * S'utilise de concert avec WriteStreamMessage().
 *
 * @param fd Descripteur de fichier vers le flux à lire.
 * @param buffer Mémoire tampon où est conservé le message
 *    lu.
 * @param maxSize Taille maximale des messages à lire en
 *    une fois.
 * @param size Contiendra la longueur des données écrites
 *    dans la mémoire tampon.
 * @param messageSuspendu Mémoire avec les messages en
 *    attente d'être affichés. Si cette mémoire n'est
 *    pas NULL, alors elle est vidée si un SIGINT est
 *    reçu durant la lecture. Si cette mémoire est NULL,
 *    la fonction ReadStreamMessage() se termine avec une
 *    erreur.
 *
 * @return true si la lecture du flux a pu être effectuée
 *    et retourne false si une erreur critique est survenue.
 **/
bool ReadStreamMessage(int fd, char* buffer, uint32_t maxSize, uint32_t* size, MessageSuspendu* messageSuspendu);

/**
 * Écrit un message `buffer` sur un flux d'octets (un
 * pipe nommé dans notre implémentation) référencé par le
 * descripteur de fichiers `fd`.
 *
 * S'utilise de concert avec ReadStreamMessage().
 *
 * @param fd Descripteur de fichier vers le flux à lire.
 * @param buffer Mémoire tampon où est conservé le message
 *    lu.
 * @param size Longueur des données écrites sur le flux.
 * @param messageSuspendu Mémoire avec les messages en
 *    attente d'être affichés. Si cette mémoire n'est
 *    pas NULL, alors elle est vidée si un SIGINT est
 *    reçu durant la lecture. Si cette mémoire est NULL,
 *    la fonction ReadStreamMessage() se termine avec une
 *    erreur.
 *
 * @return true si la lecture du flux a pu être effectuée
 *    et retourne false si une erreur critique est survenue.
 **/
bool WriteStreamMessage(int fd, char* buffer, uint32_t size, MessageSuspendu* messageSuspendu);

#endif
