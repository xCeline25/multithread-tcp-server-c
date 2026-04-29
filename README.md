# multithread-tcp-server-c
Serveur de communication réseau multi-threadé en C avec sockets TCP/IP
Freescord – Chat et transfert de fichiers en C


# Description

Freescord est une application réseau en C basée sur TCP permettant :

-discussion multi-clients (type chat)<br/>
-gestion de pseudonymes<br/>
-messages privés entre utilisateurs<br/>
-transfert de fichiers entre client et serveur<br/>
-liste des utilisateurs et des fichiers disponibles sur le serveur<br/>

Le projet repose sur une architecture client/serveur multithread.

# Fonctionnalités
## Chat
`msg <texte>` : envoi d’un message à tous les utilisateurs connectés<br/>
`whisper <pseudo> <texte>` : envoi d’un message privé à un utilisateur
## Utilisateurs
`nickname <pseudo>` : définir son pseudonyme (obligatoire à la connexion)<br/>
`list` : afficher la liste des utilisateurs connectés

## Gestion des fichiers

Upload (client vers serveur)<br/>
`fput <fichier>`
Envoie un fichier du client vers le serveur.<br/>
Le fichier est stocké dans le dossier files/ du serveur.<br/>

Download (serveur vers client)<br/>
`fget <fichier>`
Récupère un fichier depuis le serveur et le crée localement sur le client.<br/>

Liste des fichiers serveur<br/>
`flist`
Affiche les fichiers présents dans le dossier files du serveur.<br/>

# Architecture
## Client
utilise poll() pour gérer simultanément :
-entrée clavier<br/>
-socket réseau<br/>
-gère les commandes utilisateur<br/>
-utilise une socket secondaire pour les transferts de fichiers<br/>
## Serveur
-gestion multi-clients via threads (pthread)<br/>
synchronisation avec mutex pour la liste des utilisateurs
diffusion des messages via un pipe
gestion des commandes et des transferts de fichiers<br/>
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
`./srv`<br/>
Client
`./clt 127.0.0.1`

# Concepts utilisés
-sockets TCP<br/>
-programmation réseau client/serveur<br/>
-multithreading (pthread)<br/>
-mutex (synchronisation)<br/>
-communication inter-processus (pipe)<br/>
-multiplexage I/O avec poll<br/>
-gestion de fichiers<br/>
-protocole applicatif personnalisé<br/>

# Limitations
absence de chiffrement<br/>
absence d’authentification sécurisée<br/>
validation basique des entrées<br/>
gestion simple des erreurs réseau<br/>

# Auteur
Projet réalisé dans un cadre pédagogique (réseaux et systèmes).<br/>
