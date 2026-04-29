# multithread-tcp-server-c
Serveur de communication réseau multi-threadé en C avec sockets TCP/IP
Freescord – Chat et transfert de fichiers en C


# Description

Freescord est une application réseau en C basée sur TCP permettant :

-discussion multi-clients (type chat)
-gestion de pseudonymes
-messages privés entre utilisateurs
-transfert de fichiers entre client et serveur
-liste des utilisateurs et des fichiers disponibles sur le serveur

Le projet repose sur une architecture client/serveur multithread.

# Fonctionnalités
## Chat
`msg <texte>` : envoi d’un message à tous les utilisateurs connectés
`whisper <pseudo> <texte>` : envoi d’un message privé à un utilisateur
## Utilisateurs
`nickname <pseudo>` : définir son pseudonyme (obligatoire à la connexion)
`list` : afficher la liste des utilisateurs connectés

## Gestion des fichiers

Upload (client vers serveur)
`fput <fichier>`
Envoie un fichier du client vers le serveur.
Le fichier est stocké dans le dossier files/ du serveur.

Download (serveur vers client)
`fget <fichier>`
Récupère un fichier depuis le serveur et le crée localement sur le client.

Liste des fichiers serveur
`flist`
Affiche les fichiers présents dans le dossier files du serveur.

# Architecture
## Client
utilise poll() pour gérer simultanément :
-entrée clavier<br/>
-socket réseau<br/>
-gère les commandes utilisateur<br/>
-utilise une socket secondaire pour les transferts de fichiers<br/>
## Serveur
-gestion multi-clients via threads (pthread)
synchronisation avec mutex pour la liste des utilisateurs
diffusion des messages via un pipe
gestion des commandes et des transferts de fichiers
Système de transfert de fichiers<br/>

Le transfert de fichiers utilise des sockets dynamiques :<br/>

Le serveur crée une socket avec bind(port 0)<br/>
Le système attribue un port libre automatiquement
Le serveur récupère ce port avec getsockname
Le serveur envoie au client :<br/>
fput_ready <port> ou fget_ready <port>
Le client se connecte à ce port
Les données sont transférées via cette connexion dédiée

Cela permet de séparer :<br/>

le canal de chat principal
les transferts de fichiers

# Compilation
make

# ou manuellement :

`gcc -Wall -Wvla -std=c99 -pthread client.c -o clt`<br/>
`gcc -Wall -Wvla -std=c99 -pthread serveur.c -o srv`

# Exécution
Serveur
`./srv`
Client
`./clt 127.0.0.1`

# Concepts utilisés
-sockets TCP
-programmation réseau client/serveur
-multithreading (pthread)
-mutex (synchronisation)
-communication inter-processus (pipe)
-multiplexage I/O avec poll
-gestion de fichiers
-protocole applicatif personnalisé

# Limitations
absence de chiffrement
absence d’authentification sécurisée
validation basique des entrées
gestion simple des erreurs réseau

# Auteur
Projet réalisé dans un cadre pédagogique (réseaux et systèmes).
