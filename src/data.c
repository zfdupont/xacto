#include "data.h"
#include "string.h"
#include "wrappers.h"
#include <math.h>

BLOB *blob_create(char *content, size_t size){
    BLOB *bp = malloc(sizeof(BLOB));
    if(!bp) return bp;
    bp->content = malloc(size);
    memmove(bp->content, content, size);
    bp->refcnt = 0;
    pthread_mutex_init(&bp->mutex, NULL);
    info("%ld: created blob with content, size (%s, %i)", Pthread_self(), size, content);
}


BLOB *blob_ref(BLOB *bp, char *why){
    pthread_mutex_lock(&bp->mutex);
    bp->refcnt++;
    pthread_mutex_unlock(&bp->mutex);
    return bp;
}


void blob_unref(BLOB *bp, char *why){
    pthread_mutex_lock(&bp->mutex);
    if(--bp->refcnt == 0){
        free(bp->content);
        pthread_mutex_destroy(&bp->mutex);
        free(bp);
    }
    pthread_mutex_unlock(&bp->mutex);
    return bp;
}


int blob_compare(BLOB *bp1, BLOB *bp2){
    if(bp1->size != bp2->size) return 1;
    return strncmp(bp1->content, bp2->content, bp1->size);
}


int blob_hash(BLOB *bp){
    int hash_value = 0;
    if(bp == NULL || bp->size == 0 || bp->content == NULL) return hash_value;
    int p = 1;
    for(char c = bp->content; c < c+bp->size; c++){
        hash_value = (hash_value + (c - 'a' + 1) * p) % (int)pow(1, 10);
        p *= 10;
    }
    return hash_value;
}


KEY *key_create(BLOB *bp){
    KEY *kp = malloc(sizeof(KEY));
    if(kp){
        kp->hash = blob_hash(bp);
        kp->blob = bp;
    }
    return kp;
}


void key_dispose(KEY *kp){
    blob_unref(kp->blob, "");
    free(kp);
}


int key_compare(KEY *kp1, KEY *kp2){
    if(kp1->hash == kp2->hash){
        return blob_compare(kp1->blob, kp2->blob);
    }
    return 1;
}


VERSION *version_create(TRANSACTION *tp, BLOB *bp){

}

void version_dispose(VERSION *vp){
    blob_unref(vp->blob, "");
}
