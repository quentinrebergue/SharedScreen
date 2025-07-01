# Découvrir la mémoire vidéo en créant son propre partage d'écran

## Introduction

Sous Linux, l’affichage graphique résulte d’une collaboration étroite entre
le noyau, le GPU et divers services en espace utilisateur.

Cet article propose une exploration progressive de ces mécanismes, suivie
d’une mise en pratique : **la création d’un programme minimal de capture
d’écran.**

Nous passerons par les principales couches logicielles du système graphique
Linux : **framebuffer**, **DRM** (avec GEM et KMS), **compositeur graphique**,
**xdg-desktop-portal** et **PipeWire**.

Ce que vous allez apprendre :
- Ce qu’est un framebuffer et comment il est manipulé par le kernel Linux ;
- Comment une image arrive-t-elle réellement sur votre écran ;
- Comment capturer une image de l’écran de manière sécurisée ;

Ce premier article pose les fondations techniques. Nous coderons une
**version simple du client de partage d’écran**, avec une seule capture envoyée
au serveur. La capture en continu pourra faire l’objet d’un second article.

## Prérequis

Cet article s’adresse à des lecteurs ayant déjà quelques bases en informatique
et programmation système.

Pour bien suivre les explications et les exemples de code, il est recommandé
de connaître :
- les **principes de base d’un système Linux** : ce qu’est le kernel, la notion
de périphériques `/dev`, etc.
- la différence entre **userland** (espace utilisateur) et **kernel space**
(espace noyau) ;
- les fondamentaux du langage **C** : pointeurs, structures, fonctions ;
- la notion de **mémoire RAM** ;

Aucune connaissance préalable de DRM, KMS, PipeWire ou libportal n’est
nécessaire : tout sera expliqué au fur et à mesure.

## Comment sont stockées et affichées les images à l’écran ?

La mémoire vidéo correspond à la partie spécifique de la mémoire destinée à
stocker les informations à afficher à l'écran. Elle est représentée sous la
forme d'un buffer appelé **framebuffer** qui est stocké dans la mémoire
dynamique.
### Framebuffer

Un framebuffer est un espace alloué en mémoire permettant de stocker les
informations destinées à être affichées à l’écran. Chaque pixel est représenté
par 4 octets (RGBA), sa taille peut être calculée avec la formule suivante :
_taille = largeur × hauteur × 4 octets_.

Avec des fréquences de 60 à 240 Hz et des résolutions jusqu’à la 4K,
les écrans modernes imposent des calculs rapides pour actualiser continuellement
le framebuffer, ce qui nécessite un composant graphique spécialisé.

### Quel composant gère la mémoire vidéo ?

Pour effectuer ces calculs rapidement, l'ordinateur utilise une carte
graphique (GPU). Celle-ci peut être intégrée au processeur (**iGPU**) ou
externe (**GPU dédié**). Le GPU est spécialement conçu pour les calculs
graphiques.

Les GPU dédiés disposent de leur propre mémoire, la **VRAM (Video RAM)**,
conçue pour offrir une **bande passante élevée** et un **accès ultra-rapide**
ce qui permet de meilleures performances de rafraichissement.

Lorsqu’un GPU dédié est utilisé, le framebuffer est stocké dans sa
mémoire vidéo (VRAM). En l’absence de GPU et de VRAM, c’est la RAM qui héberge
le framebuffer, et l’iGPU prend alors en charge les calculs d’affichage.

### Fonctionnement général sur Linux côté Kernel

Nous avons vu que le framebuffer contient les pixels à afficher. Mais comment
ces données sont-elles envoyées concrètement à l’écran ?
Pour comprendre cela, plusieurs questions se posent : 

* Qui gère les framebuffers ?
* Comment sont-ils créés ?
* Comment le framebuffer est-il relié à l'écran ?
* Comment le curseur de notre souris est-il affiché ?

### Direct Rendering Manager (DRM)

**DRM** (*Direct Rendering Manager*) est un sous-système du noyau Linux chargé
de la gestion de l’affichage graphique. Il permet à des applications en
espace utilisateur d’interagir avec le matériel graphique de manière sécurisée
et standardisée.

Il prend en charge l’allocation des framebuffers, leur gestion,
leur association à un affichage physique et leur exposition
via des interfaces comme `/dev/dri/card0`.

DRM se divise en trois composants principaux :
- **DRM Core**
- **GEM (Graphics Execution Manager)**
- **KMS (Kernel Mode Setting)**

#### DRM Core

Le DRM core coordonne les interactions entre **GEM** et **KMS** et assure
l'interfaçage avec l'espace utilisateur via le périphérique `/dev/dri/card0`.

#### Graphical Execution Manager (GEM)

En réalité, les framebuffers sont gérés sous forme de **GEM framebuffers**,
c’est-à-dire des buffers graphiques alloués et manipulés par le gestionnaire
GEM.

GEM gère le CRUD des GEM framebuffers. Chaque framebuffer GEM est constitué :
* d'un identifiant (`gem_handle`)
* d'un buffer contenant les données des pixels (le framebuffer).

Chaque élément visible à l’écran (fenêtre, vidéo, curseur…) repose sur un
**GEM framebuffer distinct**, assemblé visuellement par KMS.

#### Kernel Mode Setting (KMS)

KMS est la partie du noyau Linux responsable de **configurer l’affichage** et
de **composer les différentes couches graphiques** à l’écran.
Il assemble les images provenant de plusieurs buffers
(fenêtres, vidéos, curseur…) et produit la sortie finale envoyée à l’écran.

Pour cela, KMS s’appuie sur plusieurs concepts clés :

##### KMS Buffer

Un **KMS Buffer** est une structure utilisée par KMS pour manipuler
un **framebuffer**. Il contient:
- un pointeur vers le framebuffer (géré par GEM) ;
- des informations complémentaires comme la taille, le format des pixels,
ou l’emplacement mémoire ;

Autrement dit, **le KMS Buffer est l’enveloppe qui décrit comment utiliser un
framebuffer dans la chaîne d’affichage gérée par KMS.**

##### Plane

Un **Plane** représente une couche d’image que le système peut afficher à
l’écran. Chaque Plane est associé à un **KMS Buffer** contenant l’image à
afficher, ainsi qu’à une **couche de composition** (layer).

Il existe plusieurs types de couches selon leur fonction :

- **Primary** : couche principale (le fond, typiquement le bureau) ;

- **Overlay** : couche intermédiaire (ex. : vidéo en lecture) ;

- **Cursor** : couche du curseur de la souris ;

KMS peut superposer plusieurs Planes pour construire l’image finale.

Représentation simplifiée :
```c
struct Plane {
    struct KMS_Buffer buffer; // Image à afficher
    enum layers layer;              // Type de couche (Primary, Overlay, Cursor)
};
```

##### CRTC, Encodeur & Connecteur

- **CRTC** (Cathode Ray Tube Controller) : est chargé de combiner les
différents Planes et de générer l’image finale envoyée à l’écran.

- **Encodeur** : adapte le signal vidéo généré par le CRTC au format
requis (HDMI, DisplayPort, etc.)

- **Connecteur** : correspond à la **prise physique** reliée à
l’écran (port HDMI, VGA, etc.)

Pour résumer :
 - **DRM core** assure l’interface entre le noyau et l’espace utilisateur ;
 - **GEM** gère les framebuffers côté kernel ;
 - **KMS** assemble ces buffers pour composer l’image finale envoyée à l’écran ;

Nous allons maintenant voir comment on peut communiquer avec DRM en userland
pour notre exemple de partage d'écran.

<TODO Insert schema de la gestion video kernel*>

## Gestion de la vidéo en userland

Nous avons vu comment le noyau (kernel) gère l'affichage les composants DRM.
Mais pour créer un programme de partage d'écran, il ne suffit pas de comprendre
le fonctionnement du kernel : il faut aussi savoir comment interagir avec la
vidéo depuis l’espace utilisateur (userland) où est stocké notre programme.

### Accès au système graphique : `/dev/dri/card0`

`/dev/dri/card0` est une interface exposée sous forme de file descriptor (fd),
qui permet la communication entre l’espace utilisateur (*userland*) et
DRM (kernel).

Pour envoyer des requêtes via cette interface, on utilise la
bibliothèque **libDRM**, qui fournit des fonctions bas-niveau pour interagir
avec DRM.

À première vue, on pourrait se dire qu’il suffit d’utiliser directement
**libDRM** pour accéder au contenu de l’écran.
Mais cela ne prend pas en compte un élément fondamental de notre système : 
**le compositeur graphique**, qui utilise lui-même **libDRM** et 
`/dev/dri/card0` pour gérer l’affichage.

Tenter d’ouvrir une connexion directe avec DRM en parallèle du compositeur
entraînerait un conflit. C’est pourquoi, dans notre cas, il est indispensable
de passer par le compositeur. Nous allons à présent voir ce qu’est un
compositeur graphique, quel rôle il joue dans le système d’affichage,
et comment notre programme peut communiquer avec lui.

### Le compositeur graphique

Le **compositeur graphique** est le programme qui gère l’interface visuelle :
il dessine les fenêtres, le curseur, et contrôle l’apparence du système.
Il s'occupe aussi de l’accès à l’affichage, empêchant toute application de
capturer l’écran sans autorisation. 

Sans lui, il n'y aurait pas de souris ni de fenêtres mais juste un terminal
texte.

Toute demande de capture d’écran doit passer par lui. Il est donc indispensable
de le prendre en compte si l’on veut créer notre programme de partage d'écran.
Les systèmes Linux utilisent principalement deux architectures graphiques :

* **X11** : Ancien protocole, avec une gestion centralisée de l'affichage.
* **Wayland** (GNOME, KDE): Plus récent, léger, sécurisé et performant,
Wayland isole les applications les unes des autres pour empêcher les accès
non autorisés à l’affichage.

Nous avons vu comment les flux vidéo sont partagés entre le noyau (via DRM) et
le compositeur graphique, ainsi que le rôle central que ce dernier joue dans
l’affichage.  
Maintenant, nous allons voir comment notre programme de partage d’écran peut
communiquer avec le compositeur pour accéder au contenu de l’écran.

### Interaction avec la mémoire vidéo

Avant de capturer l’écran, notre programme doit obtenir l’autorisation du
compositeur de lire le flux vidéo de notre écran. Une fois que le compositeur
accepte, il lui fournit de quoi s'abonner à ce flux pour recevoir la donnée
sous forme de buffers en mémoire.

La prochaine étape consiste à comprendre par quel mécanisme cette demande
d’accès est effectuée.

#### Accès sécurisé à l’écran : xdg-desktop-portal et PipeWire

Sous **Wayland**, une application ne peut pas accéder directement au contenu
de l’écran. Pour des raisons de sécurité, seul le compositeur graphique peut
autoriser ou refuser cet accès.

Or, les applications n'ont pas de moyen de communication direct avec le
compositeur. Elles doivent donc passer par une interface intermédiaire :
le portail graphique **xdg-desktop-portal**.

Ce portail agit comme un **intermédiaire sécurisé** : il transmet la demande au
compositeur, affiche une boîte de dialogue à l’utilisateur, puis relaye la
réponse (autorisation ou refus) à l’application.
Il permet ainsi à une application non privilégiée de **demander un accès
contrôlé à l’écran, avec l’accord explicite de l’utilisateur**.

Ce portail est composé de trois parties :

- **Frontend** : accessible par notre application, il expose des méthodes
simples à appeler ;

- **Daemon** : relie le frontend au backend et assure la coordination ;

- **Backend** : communique avec le compositeur dans notre cas ;

### Déroulement d’une capture d’écran

La capture d’écran via **xdg-desktop-portal** suit trois étapes :

1. `CreateSession` : une session peut être comparée à une requête et son suivi.

2. `SelectSources` : l’utilisateur choisit ce qu’il veut partager
(écran complet, fenêtre, etc.).

3. `Start` : le portail valide la demande, puis crée le **flux vidéo**
via **Pipewire**

Une fois la session démarrée, le portail renvoie deux informations :
- un **file descriptor (fd)** : une **socket Unix** permettant à notre
application de se connecter au serveur PipeWire ;
- un **node ID** : un **identifiant unique du flux vidéo**, utilisé pour
s’abonner au bon flux (celui venant d'être crée)

### PipeWire : le transport du flux

**PipeWire** est un **serveur multimédia** conçu pour transporter des flux audio
et vidéo (comme le son, la webcam ou l’écran) avec une latence très faible.

Dans notre cas, c’est **PipeWire** qui transmet les images de l’écran depuis
le compositeur vers notre application. Grâce au **file descriptor** et au
**node ID** fournis par le portail, notre programme peut se connecter au bon
flux et recevoir les images de l’écran sous forme de buffers.

**En résumé** :
**xdg-desktop-portal** sert à formuler une demande sécurisée au compositeur
pour accéder à l’écran, avec l’accord explicite de l’utilisateur.
Une fois cette demande acceptée, **PipeWire** prend le relais pour mettre en
place un flux vidéo, permettant au compositeur d’envoyer les images de l’écran
à l’application.

### Technologie simplifiante

Comme on a pu le voir, la mise en place d’une capture d’écran via
**xdg-desktop-portal** et **PipeWire** peut sembler complexe. Heureusement,
il existe des bibliothèques pour simplifier cette interaction.

#### libportal

**libportal** est une bibliothèque C qui fournit une interface simple pour
utiliser **xdg-desktop-portal**. Elle masque toutes les étapes complexes 
(création de session, sélection des sources, gestion des réponses…) derrière
une API simple.

Maintenant que nous comprenons l’ensemble du mécanisme — de la mémoire vidéo au
compositeur, en passant par le portail et PipeWire —, nous allons passer à la
pratique : 
**mettre en œuvre une première version de notre programme de partage d’écran**.

## Vers une application de partage d’écran

L’objectif de cette partie est de **commencer un programme de partage d’écran**,
en codant une **première version simplifiée** :

Le client capture une image de l’écran, la transforme en format brut (raw),
puis l’envoie au serveur via le réseau.

### Aperçu technique

Voici les grandes étapes du fonctionnement de notre application :

1. Le **client** utilise `libportal` pour capturer une image de l’écran ;
2. Il transforme cette image PNG en données brutes (pixels RGBA) ;
3. Il envoie ces données via **UDP** à un serveur ;
4. Le serveur reçoit et reconstitue l’image dans un format simple (PPM).

Pour simplifier l’implémentation, nous utiliserons :
- `libportal` : pour accéder facilement au portail **xdg-desktop-portal** ;
- `GLib/GIO` : indispensable au fonctionnement de libportal, elle fournit la
boucle d’événements asynchrone requise et constitue la base des
applications GNOME ;
- `GdkPixbuf` : pour convertir l’image PNG en raw ;

Les performances réseau sont optimisées grâce à **UDP** et **IO_uring**,
déjà implémentés dans le projet. Nous n’entrerons toutefois pas dans les
détails ici, afin de rester concentrés sur le fonctionnement global.

### Implémentation : capture côté client

Commençons par écrire une fonction `capture_screenshot()` en C, qui capture
l’écran et récupère l’image au format PNG.

Nous allons utiliser les fonctions de libportal :

- `xdp_portal_new()`: créer un XDG Portal.
- `xdp_portal_take_screenshot()`: Lance une capture d'écran async
- `xdp_portal_take_screenshot_finish()`: Fini la capture et retourne le
PNG resultant
- `XDP_PORTAL()`: Permet de cast

Ainsi que les GLIB event loop permettant de mettre en attente notre programme.

Nous allons commencer par définir une structure permettant de stocker toutes
les informations dont on aura besoin :

```C
struct {
	GMainLoop *loop;           //GLIB event loop pour gérer l'async
    guchar    *data;           //Buffer où on stocke l'image
    gsize      length;         //Taille du buffer
    guint32    width, height;  //Les dimensions de l'image
    XdpPortal *portal;         //Le portail
} screen_data;
```

Puis nous allons implémenter notre fonction `capture_screenshot()`:

```C
int capture_screenshot(struct screen_data* sd)
{
    //Init une GLIB event loop
    g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);

    //Stocke la GLIB event loop dans la struct
    sd->loop = loop;

    //Créer le portail XDG et le stocke dans la struct
    sd->portal = xdp_portal_new();

    // Démarre la capture d’écran de façon asynchrone :
    // - sd->portal   : Objet qui gère la capture (notre portail)
    // - NULL         : Fenêtre parent (aucune)
    // - XDP_SCREENSHOT_FLAG_NONE : Pas d'option
    // - NULL         : Aucun détail supplémentaire
    // - on_screenshot_ready : Callback appelé une fois la capture faite
    // - sd           : notre data qu'on veut donner à la callback
    xdp_portal_take_screenshot(sd->portal, NULL, XDP_SCREENSHOT_FLAG_NONE, NULL,
        on_screenshot_ready, sd);

    //Lance la boucle, en attente de la callback
    g_main_loop_run(loop);

    if (!sd->data)
    {
        fprintf(stderr, "La capture d'écran a échoué.\n");
        return -1;
    }
    return 0;
}
```

Puis notre callback `on_screenshot_ready()`:

```c
// Callback une fois que le screenshot a été fait
// - GObject *source : l’objet qui a lancé l’opération => sd->portal
// - GAsyncResult *result : Le résultat du screenshot
// - gpointer user_data : notre screen_data qu'on a donné en argument
void on_screenshot_ready(GObject *source, GAsyncResult *res,
    gpointer user_data)
{
    //On convertit au bon type
    struct screen_data *sd = user_data;
    GError *error = NULL;

    //Termine l'opération de screenshot async et get l'URI de l'image
    //XDP_PORTAL est simplement un cast d'un GObject* vers XdpPortal*  
    gchar *uri = xdp_portal_take_screenshot_finish(XDP_PORTAL(source), res,
    &error);

    if (error)
    {
        g_printerr("Erreur de capture d'écran: %s\n", error->message);
        g_error_free(error);
        g_main_loop_quit(sd->loop);
        return;
    }

	// Conversion d'un URI en chemin local
    gchar *path = g_filename_from_uri(uri, NULL, &error);

    // On libère l’URI
    g_free(uri);

    if (error)
    {
        g_printerr("Erreur de conversion d'URI: %s\n", error->message);
        g_error_free(error);
        g_main_loop_quit(sd->loop);
        return;
    }

    //Lit le contenu du PNG dans notre buffer sd->data et update length
    if (!g_file_get_contents(path, (gchar**)&sd->data, &sd->length, &error))
    {
        g_printerr("Échec de la lecture de %s: %s\n", path, error->message);
        g_error_free(error);
    }

    //On free le path
    g_free(path);

    //On quitte la boucle async permettant de débloquer capture_screenshot
    g_main_loop_quit(sd->loop);
}
```

### Et ensuite ?

Une fois la capture effectuée, l’image PNG est convertie en raw
(ex : tableau de pixels RGBA), puis envoyée via UDP.
Le serveur, de son côté, reçoit ces données et les écrit dans un fichier `.ppm`
pour affichage.

Le code complet (client et serveur) est disponible sur un repo GitHub associé à
l’article.

<TODO insert lien du repo GIT*>

## Conclusion

Dans cet article, nous avons exploré le fonctionnement de la mémoire vidéo sous
Linux, en partant du framebuffer jusqu’à son affichage à l’écran à travers le
sous-système DRM.
Nous avons aussi vu comment les compositeurs modernes prennent le relais dans
la gestion de l’affichage et de la sécurité, et comment une application peut
demander une capture d’écran via **xdg-desktop-portal** et **PipeWire**.

En nous appuyant sur **xdg-desktop-portal** et **PipeWire**, nous avons mis en
place une méthode sécurisée pour capturer l’écran depuis une application non
privilégiée.
Enfin, nous avons commencé l’implémentation d’un programme de
**partage d’écran minimal**, basé sur une seule capture d'écran.

Ce partage d’écran minimal n’est qu’un point de départ : peut être dans un
prochain article, on explorera comment gérer un flux vidéo en continu.

## Bibliographie

https://dri.freedesktop.org/docs/drm/
https://gitlab.freedesktop.org/mesa/drm
https://flatpak.github.io/xdg-desktop-portal/
https://pipewire.org/
https://www.kernel.org/doc/html/latest/gpu/drm-kms.html
https://docs.flatpak.org/libportal/t
https://wayland.freedesktop.org/architecture.html
https://docs.flatpak.org/en/latest/portals.html
https://gitlab.freedesktop.org/pipewire/pipewire/-/wikis/Screen-Capture
https://gitlab.freedesktop.org/pipewire/pipewire/-/wikis/home
https://developer.gnome.org/gdk-pixbuf/stable/
https://docs.gtk.org/glib/
https://drewdevault.com/2018/07/29/KMS-DRM-tutorial.html
