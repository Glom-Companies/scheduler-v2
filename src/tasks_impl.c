// src/tasks_impl.c
#define _POSIX_C_SOURCE 200809L
#include "tasks_impl.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>    // execlp, getpid, dup2, getuid
#include <fcntl.h>     // open
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define LOGFILE "/tmp/scheduler.log"

// Redirige stdout et stderr vers le fichier de log (append)
static void redirect_output_to_log(void) {
    int fd = open(LOGFILE, O_WRONLY | O_APPEND);
    if (fd == -1) {
        perror("[tasks_impl] Erreur ouverture LOGFILE pour redirection");
        return;
    }
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);
}

// Détecte si un chemin est un fichier régulier (pas un répertoire)
static int is_regular_file(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) return 0;
    return S_ISREG(st.st_mode);
}

// Retourne 1 si l'extension correspond à un format vidéo courant
static int is_video_file(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return 0;
    ext++;
    if (strcasecmp(ext, "mp4") == 0) return 1;
    if (strcasecmp(ext, "mkv") == 0) return 1;
    if (strcasecmp(ext, "avi") == 0) return 1;
    if (strcasecmp(ext, "mov") == 0) return 1;
    if (strcasecmp(ext, "webm") == 0) return 1;
    return 0;
}

// Retourne 1 si l'extension correspond à un format audio courant
static int is_audio_file(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return 0;
    ext++;
    if (strcasecmp(ext, "mp3") == 0) return 1;
    if (strcasecmp(ext, "wav") == 0) return 1;
    if (strcasecmp(ext, "flac") == 0) return 1;
    if (strcasecmp(ext, "aac") == 0) return 1;
    if (strcasecmp(ext, "ogg") == 0) return 1;
    return 0;
}

// Génère "basename_compressed.ext" pour la sortie ffmpeg
static char *make_ffmpeg_output(const char *input_path) {
    const char *dot = strrchr(input_path, '.');
    if (!dot) {
        size_t len = strlen(input_path);
        char *out = malloc(len + strlen("_compressed") + 1);
        if (!out) return NULL;
        sprintf(out, "%s_compressed", input_path);
        return out;
    }
    size_t base_len = dot - input_path;
    const char *ext = dot + 1;
    size_t ext_len = strlen(ext);
    // Construire "basename_compressed.ext"
    char *out = malloc(base_len + strlen("_compressed.") + ext_len + 1);
    if (!out) return NULL;
    memcpy(out, input_path, base_len);
    strcpy(out + base_len, "_compressed.");
    strcpy(out + base_len + strlen("_compressed.") , ext);
    return out;
}

// ====== Compression de fichier ======
static void task_compress(Task *t) {
    redirect_output_to_log();
    const char *inPath = t->param1;
    const char *outPath = t->param2; // sert uniquement pour zstd

    // Cas 1 : si c'est un fichier audio/vidéo → ffmpeg
    if (is_regular_file(inPath) && (is_video_file(inPath) || is_audio_file(inPath))) {
        char *ffout = make_ffmpeg_output(inPath);
        if (!ffout) {
            fprintf(stderr, "[tasks_impl] Erreur allocation pour ffmpeg output\n");
            _exit(EXIT_FAILURE);
        }

        if (is_video_file(inPath)) {
            // Réencoder la vidéo : CRF à 28 pour réduire significativement la taille
            execlp("ffmpeg", "ffmpeg",
                   "-i", inPath,
                   "-c:v", "libx264", "-crf", "28", "-preset", "medium",
                   "-c:a", "aac", "-b:a", "128k",
                   ffout,
                   (char *)NULL);
            fprintf(stderr, "[tasks_impl] execlp ffmpeg (video) failed: %s\n", strerror(errno));
        } else {
            // Fichier audio → réencoder en MP3 128k
            execlp("ffmpeg", "ffmpeg",
                   "-i", inPath,
                   "-c:a", "libmp3lame", "-b:a", "128k",
                   ffout,
                   (char *)NULL);
            fprintf(stderr, "[tasks_impl] execlp ffmpeg (audio) failed: %s\n", strerror(errno));
        }
        free(ffout);
        _exit(EXIT_FAILURE);
    }

    // Cas 2 : autre(s) fichier(s) ou dossier → zstd
    char *zstd_out = NULL;
    if (!outPath || strcmp(outPath, inPath) == 0) {
        size_t len = strlen(inPath);
        zstd_out = malloc(len + 5);
        if (!zstd_out) {
            fprintf(stderr, "[tasks_impl] Erreur allocation pour zstd output\n");
            _exit(EXIT_FAILURE);
        }
        sprintf(zstd_out, "%s.zst", inPath);
        outPath = zstd_out;
    }

    char threads_opt[16], level_opt[16];
    snprintf(threads_opt, sizeof(threads_opt), "-T%d", 1);
    snprintf(level_opt, sizeof(level_opt), "-%d", 3);

    execlp("zstd", "zstd",
           threads_opt, level_opt,
           inPath,
           "-o", outPath,
           (char *)NULL);
    fprintf(stderr, "[tasks_impl] execlp zstd failed: %s\n", strerror(errno));
    if (zstd_out) free(zstd_out);
    _exit(EXIT_FAILURE);
}

// ====== Conversion vidéo → audio ======
static void task_convert(Task *t) {
    redirect_output_to_log();
    execlp("ffmpeg", "ffmpeg",
           "-i", t->param1,
           "-q:a", "0", "-map", "a",
           t->param2,
           (char *)NULL);
    fprintf(stderr, "[tasks_impl] execlp ffmpeg conversion failed: %s\n", strerror(errno));
    _exit(EXIT_FAILURE);
}

// ====== Mise à jour du système ======
static void task_update(Task *t) {
    (void)t;
    redirect_output_to_log();

    if (getuid() == 0) {
        // Si on est root, on n'utilise pas sudo
        int code = system("apt update && apt upgrade -y");
        if (code == -1) {
            fprintf(stderr, "[tasks_impl] system \"apt update && apt upgrade -y\" failed: %s\n", strerror(errno));
            _exit(EXIT_FAILURE);
        }
        _exit(EXIT_SUCCESS);
    }

    // Sinon, on tente en mode non interactif,
    // si l'utilisateur a mis NOPASSWD dans sudoers pour apt
    {
        int code = system("sudo -n apt update && sudo -n apt upgrade -y");
        if (code == -1) {
            fprintf(stderr, "[tasks_impl] system \"sudo -n apt update && sudo -n apt upgrade -y\" failed: %s\n", strerror(errno));
            _exit(EXIT_FAILURE);
        }
        _exit(EXIT_SUCCESS);
    }
}

// ====== Clonage Git ======
static void task_clone(Task *t) {
    redirect_output_to_log();
    // Désactiver prompt SSH (passphrase) :
    setenv("GIT_TERMINAL_PROMPT", "0", 1);

    execlp("git", "git", "clone", t->param1, t->param2, (char *)NULL);
    fprintf(stderr, "[tasks_impl] execlp git clone failed: %s\n", strerror(errno));
    _exit(EXIT_FAILURE);
}

void execute_task(Task *t) {
    switch (t->type) {
        case TASK_COMPRESS:
            task_compress(t);
            break;
        case TASK_CONV_VIDEO:
            task_convert(t);
            break;
        case TASK_UPDATE:
            task_update(t);
            break;
        case TASK_CLONE:
            task_clone(t);
            break;
        default:
            fprintf(stderr, "[tasks_impl] Type de tâche inconnu: %d\n", t->type);
            _exit(EXIT_FAILURE);
    }
}
