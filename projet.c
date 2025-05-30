# include <stdlib.h>
# include <stdio.h>
# include <sys/stat.h>
# include <sys/wait.h>
# include <sys/types.h>
# include <string.h>
# include <zstd.h>


void flush_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}


static void remove_newline(char *s) {
    size_t len = strlen(s);
    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }
}  // fonction de gpt pour effacer le /n



#define CHUNK_SIZE 131072 // débit de lecture et d'écriture des données : 128 Kilobites

int compress_file( const char *inPath , const char *outPath , int nbThreads , int niveau) {
FILE *ficher_in = fopen(inPath , "rb");
FILE *ficher_out = fopen(outPath , "wb");

if ((ficher_in ==  NULL) || (ficher_out == NULL) ){
printf("pitainn il y a erreur");
return -1;
}

ZSTD_CCtx* cctx = ZSTD_createCCtx(); /*  le cctx est un objet de la bibliothèque Zstandard. ca permet de définir les paramètres de la compression , donc le nombre de
threads à utiliser , le niveau de compression , etc              */
ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, nbThreads); /* ici se fait la création des threads en fonction de ce que l'utilisateur a chosisi. si c'est un seul
threads , le programme decoupe le ficher successivement en bloc et le traite. S'il y a plusieurs threads alors les blocs sont confiés aux threads qui les traitent
en parallèle.  */
ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, niveau); /* on définit le niveau de compression. Le niveau de compression permet de compromis entre vitesse
de compression et taux de compression. Plus le niveau choisi est bas plus la compression est rapide et plus le niveau choisi est élevé plus le taux de compression est
élevé mais la vitesse diminue considérablement.  Au delà de 10 le gain de taux de compression devient minime.  */

/* Maintenant on utilise la fonction ZSTD_compressStream() pour faire la compression. Donc on lit un bloc du ficher, dans notre cas , 128 KIb. Cette portion est
compressé et écrite dans le ficher de destination. Et on répète cette action jusqu'à ce que tous le ficher soit compresser.         */


ZSTD_inBuffer  input  = { malloc(CHUNK_SIZE), 0, 0 };
ZSTD_outBuffer output = { malloc(ZSTD_CStreamOutSize()), 0, ZSTD_CStreamOutSize() };

size_t readBytes;
    while ((readBytes = fread(input.src, 1, CHUNK_SIZE, ficher_in)) > 0) {
        input.size = readBytes; input.pos = 0;
        while (input.pos < input.size) {
            size_t ret = ZSTD_compressStream(cctx, &output, &input);
            fwrite(output.dst, 1, output.pos, ficher_out);
            output.pos = 0;
        }
    }

size_t remaining;
do {
    remaining = ZSTD_endStream(cctx, &output);
    if (ZSTD_isError(remaining)) {
        fprintf(stderr, "Erreur pendant la fin de compression : %s\n", ZSTD_getErrorName(remaining));
        free(input.src);
        free(output.dst);
        ZSTD_freeCCtx(cctx);
        fclose(ficher_in);
        fclose(ficher_out);
        return -1;
    }
    fwrite(output.dst, 1, output.pos, ficher_out);
    output.pos = 0;
} while (remaining != 0);


    
}



ZSTD_freeCCtx(cctx);
free(input.src);
free(output.dst);

fclose(ficher_in);
fclose(ficher_out);
return 0;
}

void compression() {

char inPath[512] , outPath[512]; // variable pour prendre le chemin d'accès du ficher à comprimer et créer le ficher comprimé en ajoutant ".zst"
printf("\nEntrez le chemin d'accès du ficher à compresser : ");
fgets(inPath , sizeof(inPath) , stdin);
remove_newline(inPath); // supprimer le /n

FILE *ficher = fopen(inPath , "rb" ); // ouverture du ficher. "r" pour read et "b" pour binaire donc on lit le ficher en mode binaire
if (ficher == NULL ) {
printf("\nficher introuvable \n");
exit(1);
}

snprintf(outPath , sizeof(outPath) , "%s.zst" , inPath); /* ajouter .zst au ficher entrée par l'utilisateur. C'est dans ce ficher.zst que les données comprimées seront
écrites .  */

fclose(ficher); // on ferme le descripteur de ficher


int niveau = 3;
int nbThreads = 1;
printf("Entrez le nombre de threads que vous voulez utilisez : ");
scanf("%d" , &nbThreads);
flush_stdin();

printf("Choissisez un niveau(Entre 1 et 12) de compression(Nous vous recommendons le niveau 3 ): ");
scanf("%d" , &niveau);
flush_stdin();

 int retour = compress_file(inPath , outPath , nbThreads , niveau);
printf("\nCompression en cours\n ");

if (retour == 0) {
printf("\n Compression Réussie \n");
}
}

                                               /*Convertiseur de ficher */


int est_fichier_video(const char *chemin) {
    char commande[512];
    snprintf(commande, sizeof(commande),
             "ffprobe -v error -select_streams a:0 -show_entries stream=codec_type "
             "-of default=noprint_wrappers=1:nokey=1 \"%s\" > temp.txt 2>/dev/null", chemin);

    int retour = system(commande);
    if (retour != 0) return 0;

    FILE *f = fopen("temp.txt", "r");
    if (!f) return 0;

    char ligne[16];
    int valide = 0;
    while (fgets(ligne, sizeof(ligne), f)) {
        if (strncmp(ligne, "audio", 5) == 0 || strncmp(ligne, "video", 5) == 0) {
            valide = 1;
            break;
        }
    }

    fclose(f);
    remove("temp.txt");
    return valide;
}

void conversion() {
    char chemin_video[256];
    char nom_sortie[128];
    char mode[4];
    char choix_duree[4];
    char debut[32] = "";
    char duree[32] = "";
    char commande[1024];

    printf("Entrez le chemin du fichier vidéo : ");
    fgets(chemin_video , sizeof(chemin_video) , stdin);
    remove_newline(chemin_video);

FILE *fichier = fopen(chemin_video, "rb");
    if (!fichier) {
        printf("Erreur : fichier inexistant ou illisible.\n");
        return;
    }
    fclose(fichier);

    if (!est_fichier_video(chemin_video)) {
        printf("Erreur : ce fichier ne contient pas de flux audio ou vidéo.\n");
        return;
    }

    printf("Entrez le nom du fichier de sortie (sans .mp3) : ");
    fgets(nom_sortie , sizeof(nom_sortie) , stdin);
    remove_newline(nom_sortie);

    printf("Mode audio (CBR ou VBR) : ");
    scanf("%s", mode);
    flush_stdin();
    for (int i = 0; mode[i]; i++) mode[i] = toupper(mode[i]);

    printf("Souhaitez-vous convertir :\n");
    printf("1. Toute la vidéo\n");
    printf("2. Seulement les premières minutes\n");
    printf("3. Seulement les dernières minutes\n");
    printf("4. Une partie spécifique (à partir de t pendant d)\n");
    printf("Votre choix (1/2/3/4) : ");
    scanf("%s", choix_duree);
    flush_stdin();

    if (strcmp(choix_duree, "2") == 0) {
        printf("Durée à convertir depuis le début (ex: 00:02:00 pour 2 min) : ");
        scanf("%s", duree);
        flush_stdin();
        snprintf(commande, sizeof(commande), "ffmpeg -ss 0 -t %s -i \"%s\"", duree, chemin_video);
    } else if (strcmp(choix_duree, "3") == 0) {
        printf("Durée des dernières minutes à extraire (ex: 00:02:00) : ");
        scanf("%s", duree);
        flush_stdin();
        snprintf(commande, sizeof(commande), "ffmpeg -sseof -%s -i \"%s\"", duree, chemin_video);
    } else if (strcmp(choix_duree, "4") == 0) {
        printf("Heure de début (ex: 00:01:30) : ");
        scanf("%s", debut);
        flush_stdin();
        printf("Durée à convertir à partir de ce point (ex: 00:00:45) : ");
        scanf("%s", duree);
        flush_stdin();
        snprintf(commande, sizeof(commande), "ffmpeg -ss %s -t %s -i \"%s\"", debut, duree, chemin_video);
    } else {
        snprintf(commande, sizeof(commande), "ffmpeg -i \"%s\"", chemin_video);  // toute la vidéo
    }

    // Ajout du mode audio
    if (strcmp(mode, "CBR") == 0) {
        strcat(commande, " -b:a 192k -map a ");
    } else if (strcmp(mode, "VBR") == 0) {
        strcat(commande, " -q:a 0 -map a ");
    } else {
        printf("Mode invalide. Choisissez CBR ou VBR.\n");
        return;
    }

    char nom_complet[256];
    snprintf(nom_complet, sizeof(nom_complet), "\"%s.mp3\"", nom_sortie);
    strcat(commande, nom_complet);

    printf("Lancement de la conversion...\n");
    int retour = system(commande);

    if (retour == 0) {
        printf("Conversion terminée. Fichier créé : %s.mp3\n", nom_sortie);
    } else {
        printf("Erreur lors de la conversion.\n");
    }
}





int main() {
int choix = 0;
printf("\n----------MENU-----------\n-");
printf("\n1.Compresseur de ficher\n");
printf("\n2.Convertisseur de vidéo");
printf("\nFaites un choix (Entre 1 et 2): ");
scanf("%d",&choix );
flush_stdin();

switch (choix) {
case 1 : 
compression();
break;

case 2 : 
conversion();
break;

default :
printf("Choix invalide");
break;


}





/*  

  biblio à inclure : #include <zstd.h>
instalation : sudo apt install libzstd-dev
et pour la compilation ajoute estpace -lzstd
*/
return 0;
}
