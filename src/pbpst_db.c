#include "pbpst_db.h"
#include "pb.h"

signed
print_err2 (const char * restrict action, const char * restrict explanation) {

    signed ret = fprintf(stderr, "pbpst: %s: %s\n", action, explanation);
    return ret;
}

signed
print_err3 (const char * restrict str1, const char * restrict str2,
            const char * restrict str3) {

    signed ret = fprintf(stderr, "pbpst: %s (%s): %s\n", str1, str2, str3);
    return ret;
}

char *
db_locate (const struct pbpst_state * s) {

    char * dbl, * db = 0;
    enum {
        FLE = 0, USR = 1, XDG = 2, HME = 3
    } which_brnch = (dbl = s->dbfile)                 ? USR
                  : (dbl = getenv("XDG_CONFIG_HOME")) ? XDG
                  : (dbl = getenv("HOME"))            ? HME : FLE;

    if ( which_brnch == FLE ) {
        pbpst_err(_("No valid location available for database"));
        return 0;
    }

    signed errsv;
    size_t db_len;
    if ( which_brnch != USR ) {
        char cwd [PATH_MAX] = { '\0' };
        errno = 0;
        if ( !getcwd(cwd, PATH_MAX - 1) ) {
            errsv = errno;
            print_err2(_("Could not save working directory"), strerror(errsv));
            return 0;
        }

        errno = 0;
        if ( chdir(dbl) == -1 ) {
            errsv = errno;
            print_err3(_("Could not cd to directory"), dbl, strerror(errsv));
            return 0;
        }

        for ( uint8_t i = 0; i < 2; i ++ ) {
            i += which_brnch == XDG;
            char * str = !i ? ".config" : "pbpst";

            errno = 0;
            if ( chdir(str) == -1 ) {
                errsv = errno;
                if ( errsv == ENOENT ) {
                    errno = 0;
                    if ( mkdir(str, 0777) == -1 ) {
                        errsv = errno;
                        print_err2(_("Could not create directory"), strerror(errsv));
                        return 0;
                    }
                } else {
                    print_err2(_("Could not cd to directory"), strerror(errsv));
                    return 0;
                }
            }
        }

        errno = 0;
        if ( chdir(cwd) == -1 ) {
            errsv = errno;
            print_err3(_("Could not cd to directory"), cwd, strerror(errsv));
            return 0;
        }

        db_len = strlen(dbl) + 23;
        db = (char * )malloc(db_len);
        if ( !db ) {
            print_err2(_("Could not save db path"), _("Out of Memory"));
            return 0;
        }

        snprintf(db, db_len, "%s%s/pbpst/db.json", dbl,
                 which_brnch == XDG ? "" : "/.config");
    }

    char * fdb = which_brnch == USR ? dbl : db;

    signed fd;
    errno = 0;
    if ( (fd = open(fdb, O_CREAT | O_EXCL, 0666)) == -1 ) {
        errsv = errno;
        if ( errsv == EEXIST ) {
            return fdb;
        } else {
            print_err3(_("Could not open file"), fdb, strerror(errsv));
            free(fdb);
            return 0;
        }
    }

    errno = 0;
    if ( close(fd) == -1 ) {
        errsv = errno;
        print_err3(_("Could not close file"), fdb, strerror(errsv));
        free(fdb);
        return 0;
    } return fdb;
}

char *
db_swp_init (const char * dbl) {

    size_t len = strlen(dbl) + 1;
    char * parent = 0, * file = 0, * fc = 0, * swp_db_name = 0,
         * pc = (char * )malloc(len);

    signed fd = 0;

    if ( !pc ) {
        print_err2(_("Could not store db dirname"), _("Out of Memory"));
        fd = -1; goto cleanup;
    }

    fc = (char * )malloc(len);
    if ( !fc ) {
        print_err2(_("Could not store db basename"), _("Out of Memory"));
        fd = -1; goto cleanup;
    }

    snprintf(pc, len, "%s", dbl);
    snprintf(fc, len, "%s", dbl);

    parent = dirname(pc);
    file = basename(fc);

    char cwd [PATH_MAX] = { '\0' };

    signed errsv = 0;
    errno = 0;
    if ( !getcwd(cwd, PATH_MAX - 1) ) {
        errsv = errno;
        print_err2(_("Could not save working directory"), strerror(errsv));
        fd = -1; goto cleanup;
    }

    errno = 0;
    if ( chdir(parent) == -1 ) {
        errsv = errno;
        print_err2(_("Could not cd to database path"), strerror(errsv));
        fd = -1; goto cleanup;
    }

    len = strlen(dbl) + 7;
    swp_db_name = (char * )malloc(len);
    if ( !swp_db_name ) {
        print_err2(_("Could not save swap db name"), _("Out of Memory"));
        fd = -1; goto cleanup;
    }

    snprintf(swp_db_name, len, "%s/.%s.swp", parent, file);

    errno = 0;
    if ( (fd = open(swp_db_name, O_CREAT | O_EXCL, 0666)) == -1 ) {
        errsv = errno;
        print_err2(_("Could not create swap db"), strerror(errsv));
        fputs(_("Ensure that pbpst is not already running and that all pastes have been saved"),
              stderr);
        print_err2(_("Please remove the swap database"), swp_db_name);
        fd = -1; goto cleanup;
    }

    errno = 0;
    if ( close(fd) == -1 ) {
        errsv = errno;
        print_err3(_("Could not close file"), swp_db_name, strerror(errsv));
        fd = -1; goto cleanup;
    }

    errno = 0;
    if ( chdir(cwd) == -1 ) {
        errsv = errno;
        print_err3(_("Could not cd to directory"), cwd, strerror(errsv));
    }

    cleanup:
        free(pc);
        free(fc);
        if ( fd == -1 ) {
            free(swp_db_name);
            return 0;
        } else {
            return swp_db_name;
        }
}

signed
db_swp_cleanup (const char * restrict dbl, const char * restrict s_dbl) {

    errno = 0;
    if ( rename(s_dbl, dbl) == -1 ) {
        signed errsv = errno;
        print_err2(_("Could not overwrite database"), strerror(errsv));
        return -1;
    } return 0;
}

json_t *
db_read (const char * dbl) {

    FILE * f;
    signed errsv;
    errno = 0;
    if ( !(f = fopen(dbl, "r")) ) {
        errsv = errno;
        print_err3(_("Could not open file for reading"), dbl, strerror(errsv));
        return 0;
    }

    json_error_t err;
    json_t * mdb = json_loadf(f, 0, &err);
    if ( !mdb ) {
        errno = 0;
        if ( (fseek(f, 0, SEEK_END)) == -1 ) {
            errsv = errno;
            print_err3(_("Could not seek to file end"), dbl, strerror(errsv));
            goto cleanup;
        }

        signed long size = 0;
        errno = 0;
        if ( (size = ftell(f)) == -1 ) {
            errsv = errno;
            print_err3(_("Could not get file position"), dbl, strerror(errsv));
            goto cleanup;
        }

        if ( size == 0 ) { mdb = DEF_DB(); goto cleanup; }

        print_err3(_("Could not read file"), dbl, err.text);
        goto cleanup;
    }

    cleanup:
        errno = 0;
        if ( fclose(f) == -1 ) {
            errsv = errno;
            print_err3(_("Could not close file"), dbl, strerror(errsv));
            json_decref(mdb); mdb = 0;
        } return mdb;
}

signed
db_swp_flush (const json_t * mdb, const char * restrict s_dbl) {

    FILE * swp_db;
    signed errsv;
    errno = 0;
    if ( !(swp_db = fopen(s_dbl, "w")) ) {
        errsv = errno;
        print_err2(_("Could not open swap db"), strerror(errsv));
        return -1;
    }

    signed ret = 0;
    if ( json_dumpf(mdb, swp_db, JSON_PRESERVE_ORDER | JSON_INDENT(2)) == -1 ) {
        ret = -1;
    }

    errno = 0;
    if ( fflush(swp_db) == EOF ) {
        errsv = errno;
        print_err2(_("Could not flush memory to swap db"), strerror(errsv));
        ret = -1;
    }

    errno = 0;
    if ( fclose(swp_db) == -1 ) {
        errsv = errno;
        print_err2(_("Could not flush memory to swap db"), strerror(errsv));
        ret = -1;
    } return ret;
}

signed
pbpst_db (const struct pbpst_state * s) {

    return s->init  ? EXIT_SUCCESS                         :
           s->lspv  ? db_list_providers()                  :
           s->query ? db_query(s)                          :
           s->del   ? db_remove_entry(s->provider, s->del) :
           s->dfpv  ? db_set_default(s->provider)          :
           s->prun  ? pb_prune(s) != CURLE_OK              :
                      EXIT_FAILURE                         ;
}

signed
db_set_default (const char * provider) {

    json_t * str = json_string(provider);
    signed stat = json_object_set_new(mem_db, "default_provider", str);
    if ( !stat ) {
        print_err2(_("New provider set"), provider);
        return EXIT_SUCCESS;
    } else {
        print_err2(_("Could not set new provider"), "unknown");
        return EXIT_FAILURE;
    }
}

signed
db_add_entry (const struct pbpst_state * s, const char * userdata) {

    json_error_t err;
    json_t * json = json_loads(userdata, 0, &err);
    if ( !json ) {
        fprintf(stderr, "pbpst: %s: %d,%d\n", err.text, err.line, err.column);
        return EXIT_FAILURE;
    }

    signed status = EXIT_SUCCESS;
    pastes = json_object_get(mem_db, "pastes");
    json_incref(pastes);
    json_t * prov_obj = 0, * uuid_j = 0, * lid_j = 0,
           * label_j = 0, * status_j = 0, * sunset_j = 0, * new_paste = 0;

    char * sunset = 0;

    if ( !pastes ) { status = EXIT_FAILURE; goto cleanup; }
    prov_pastes = json_object_get(pastes, s->provider);
    json_incref(prov_pastes);
    if ( !prov_pastes ) {
        prov_obj = json_pack("{s:{}}", s->provider);
        json_object_update(pastes, prov_obj);
        json_decref(prov_obj);
        prov_pastes = json_object_get(pastes, s->provider);
    }

    uuid_j   = json_object_get(json, "uuid");
    lid_j    = json_object_get(json, "long");
    label_j  = json_object_get(json, "label");
    status_j = json_object_get(json, "status");
    sunset_j = json_object_get(json, "sunset");
    json_incref(uuid_j);
    json_incref(lid_j);
    json_incref(label_j);
    json_incref(status_j);
    json_incref(sunset_j);

    if ( !status_j ) { status = EXIT_FAILURE; goto cleanup; }
    const char stat = json_string_value(status_j)[0];
    if ( stat == 'a' ) {
        pbpst_err(_("Paste already existed"));
        goto cleanup;
    }

    if ( sunset_j && s->secs ) {
        time_t curtime = time(NULL), offset = 0;
        if ( sscanf(s->secs, "%ld", &offset) == EOF ) {
            signed errsv = errno;
            print_err2(_("Could not scan offset"), strerror(errsv));
            status = EXIT_FAILURE; goto cleanup;
        }

        if ( !(sunset = malloc(12)) ) {
            print_err2(_("Could not store sunset epoch"), _("Out of Memory"));
            status = EXIT_FAILURE; goto cleanup;
        } snprintf(sunset, 11, "%ld", curtime + offset);
    }

    if ( (!uuid_j && !s->uuid) || !lid_j ) {
        status = EXIT_FAILURE;
        goto cleanup;
    }

    const char * uuid  = uuid_j ? json_string_value(uuid_j) : s->uuid,
               * lid   = json_string_value(lid_j),
               * label = json_string_value(label_j),
               * msg   =  s->msg            ? s->msg
                       : !s->msg && s->path ? s->path : "-";

    new_paste = json_pack("{s:s,s:s,s:s?,s:s?}", "long", lid, "msg", msg,
                          "label", label, "sunset", sunset);

    if ( json_object_set(prov_pastes, uuid, new_paste) == -1 ) {
        pbpst_err(_("Could not save new paste object"));
        status = EXIT_FAILURE; goto cleanup;
    }

    cleanup:
        if ( sunset ) { free(sunset); }
        json_decref(json);
        json_decref(uuid_j);
        json_decref(lid_j);
        json_decref(label_j);
        json_decref(status_j);
        json_decref(new_paste);
        return status;
}

signed
db_remove_entry (const char * provider, const char * uuid) {

    signed status = EXIT_SUCCESS;
    pastes = json_object_get(mem_db, "pastes");
    json_incref(pastes);

    if ( !pastes ) { status = EXIT_FAILURE; goto cleanup; }
    prov_pastes = json_object_get(pastes, provider);
    json_incref(prov_pastes);

    if ( !prov_pastes ) {
        print_err2(_("No pastes in-database found for provider"), provider);
        status = EXIT_FAILURE; goto cleanup;
    }

    if ( json_object_del(prov_pastes, uuid) ) {
        print_err2(_("No paste in-database found with uuid"), uuid);
        status = EXIT_FAILURE;
    }

    cleanup:
        return status;
}

signed
db_query (const struct pbpst_state * s) {

    signed status = EXIT_SUCCESS;
    pastes = json_object_get(mem_db, "pastes");
    json_incref(pastes);

    if ( !pastes ) { status = EXIT_FAILURE; goto cleanup; }
    prov_pastes = json_object_get(pastes, s->provider);
    json_incref(prov_pastes);

    if ( !prov_pastes ) {
        print_err2(_("No pastes found for provider"), s->provider);
        status = EXIT_SUCCESS; goto cleanup;
    }

    const char * key;
    json_t * val;
    json_object_foreach (prov_pastes, key, val) {
        json_t * jl = json_object_get(val, "long"),
               * js = json_object_get(val, "sunset"),
               * jm = json_object_get(val, "msg"),
               * jv = json_object_get(val, "label");

        const char * l = json_string_value(jv) ? json_string_value(jv)
                                               : json_string_value(jl),
                   * u = json_string_value(js) ? json_string_value(js)
                                               : "N/A",
                   * m = json_string_value(jm) ;

        size_t len = strlen(key)            +
                     strlen(s->provider)    +
                     strlen(l)              +
                     json_string_length(js) +
                     json_string_length(jm) + 9;

        char * outstr = malloc(len + 1);
        if ( !outstr ) {
            print_err2(_("Could not format output"), _("Out of Memory"));
            status = EXIT_FAILURE; goto cleanup;
        }

        snprintf(outstr, len, "%s\t%s%s\t%s\t%s\n", key, s->provider, l, m, u);
        if ( strstr(outstr, s->query) ) { printf("%s", outstr); }
        free(outstr);
    }

    cleanup:
        return status;
}

signed
db_list_providers (void) {

    pastes = json_object_get(mem_db, "pastes");
    if ( !pastes ) { return EXIT_FAILURE; }

    const char * key;
    json_t * val;
    json_object_foreach (pastes, key, val) {
        puts(key);
    } return EXIT_SUCCESS;
}

// vim: set ts=4 sw=4 et:
