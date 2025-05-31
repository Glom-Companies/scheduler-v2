// src/tasks_impl.c
#define _POSIX_C_SOURCE 200809L
#include "tasks_impl.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>    // execlp, getpid, dup2
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

// Détecte si un chemin est un fichier (et non un dossier)
static int is_regular_file(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) return 0;
    return S_ISREG(st.st_mode);
}

// Retourne 1 si l'extension correspond à un format vidéo
static int is_video_file(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return 0;
    ext++; // sauter le '.'
    if (strcasecmp(ext, "mp4") == 0) return 1;
    if (strcasecmp(ext, "mkv") == 0) return 1;
    if (strcasecmp(ext, "avi") == 0) return 1;
    if (strcasecmp(ext, "mov") == 0) return 1;
    return 0;
}

// Retourne 1 si l'extension correspond à un format audio
static int is_audio_file(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return 0;
    ext++;
    if (strcasecmp(ext, "mp3") == 0) return 1;
    if (strcasecmp(ext, "wav") == 0) return 1;
    if (strcasecmp(ext, "flac") == 0) return 1;
    if (strcasecmp(ext, "aac") == 0) return 1;
    return 0;
}

// Si c'est un fichier audio/vidéo, on retourne le chemin de sortie auto (temporaire)
// en ajoutant "_compressed" juste avant l'extension. Ex: "video.mp4" -> "video_compressed.mp4"
static char *make_ffmpeg_output(const char *input_path) {
    const char *dot = strrchr(input_path, '.');
    if (!dot) {
        // Pas d'extension, on ajoute "_compressed"
        size_t len = strlen(input_path);
        char *out = malloc(len + strlen("_compressed") + 1 + 1);
        if (!out) return NULL;
        sprintf(out, "%s_compressed", input_path);
        return out;
    }
    size_t base_len = dot - input_path;
    const char *ext = dot + 1;
    size_t ext_len = strlen(ext);
    // Construire "basename_compressed.ext"
    char *out = malloc(base_len + strlen("_compressed") + 1 + ext_len + 1);
    if (!out) return NULL;
    memcpy(out, input_path, base_len);
    strcpy(out + base_len, "_compressed.");
    strcpy(out + base_len + strlen("_compressed.") , ext);
    return out;
}

// ====== Compression Fichier ======

static void task_compress(Task *t) {
    redirect_output_to_log();
    const char *inPath = t->param1;
    const char *outPath = t->param2; // peut être NULL si on veut auto-générer
    int ret;

    // Si c'est un fichier audio ou vidéo, on utilise ffmpeg pour réencoder
    if (is_regular_file(inPath) && (is_video_file(inPath) || is_audio_file(inPath))) {
        // On génère un fichier de sortie si outPath est NULL ou identique à inPath
        char *ffout = NULL;
        if (!outPath || strcmp(outPath, inPath) == 0) {
            ffout = make_ffmpeg_output(inPath);
            if (!ffout) {
                fprintf(stderr, "[tasks_impl] Erreur malloc pour ffmpeg output\n");
                _exit(EXIT_FAILURE);
            }
        } else {
            ffout = strdup(outPath);
            if (!ffout) {
                fprintf(stderr, "[tasks_impl] Erreur alloc pour outPath\n");
                _exit(EXIT_FAILURE);
            }
        }

        // Choisissez un bon réglage :
        // - Si c'est vidéo → codec libx264, CRF 23
        // - Si c'est audio → codec libmp3lame, 192k
        if (is_video_file(inPath)) {
            execlp("ffmpeg", "ffmpeg",
                   "-i", inPath,
                   "-c:v", "libx264", "-crf", "23", "-preset", "slow",
                   "-c:a", "aac", "-b:a", "128k",
                   ffout,
                   (char *)NULL);
            fprintf(stderr, "[tasks_impl] execlp ffmpeg video failed: %s\n", strerror(errno));
        } else {
            // Audio
            execlp("ffmpeg", "ffmpeg",
                   "-i", inPath,
                   "-c:a", "libmp3lame", "-b:a", "192k",
                   ffout,
                   (char *)NULL);
            fprintf(stderr, "[tasks_impl] execlp ffmpeg audio failed: %s\n", strerror(errno));
        }
        free(ffout);
        _exit(EXIT_FAILURE);
    }

    // Sinon, on retombe sur zstd (compression générique)
    if (!outPath) {
        // Générer outPath automatiquement
        size_t len = strlen(inPath);
        char *auto_out = malloc(len + 5); // ".zst" + '\0'
        if (!auto_out) {
            fprintf(stderr, "[tasks_impl] Erreur malloc pour zstd output\n");
            _exit(EXIT_FAILURE);
        }
        sprintf(auto_out, "%s.zst", inPath);
        outPath = auto_out;
    }

    // Appel zstd classique
    char threads_opt[16], level_opt[16];
    snprintf(threads_opt, sizeof(threads_opt), "-T%d", 1);
    snprintf(level_opt, sizeof(level_opt), "-%d", 3);

    execlp("zstd", "zstd",
           threads_opt, level_opt,
           inPath,
           "-o", outPath,
           (char *)NULL);
    fprintf(stderr, "[tasks_impl] execlp zstd failed: %s\n", strerror(errno));
    _exit(EXIT_FAILURE);
}

// ====== Conversion Vidéo → Audio ======

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

// ====== Mise à jour Système ======

static void task_update(Task *t) {
    (void)t;
    redirect_output_to_log();
    int code = system("sudo apt update && sudo apt upgrade -y");
    if (code == -1) {
        fprintf(stderr, "[tasks_impl] system apt update/upgrade failed: %s\n", strerror(errno));
        _exit(EXIT_FAILURE);
    }
    _exit(EXIT_SUCCESS);
}

// ====== Clonage Git ======

static void task_clone(Task *t) {
    redirect_output_to_log();
    // Correction : on appelle vraiment "git clone", pas seulement "git"
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
