/*This file is part of Moonlight Embedded.
 
  Copyright (C) 2015-2017 Iwan Timmer
 
  Moonlight is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.
 
  Moonlight is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with Moonlight; if not, see <http://www.gnu.org/licenses/>.*/

#include "docurl.h"
#include "parsexml.h"
#include "cryptssl.h"
#include "base.h"
#include "errorlist.h"

//#include <Limelight.h>

#include <sys/stat.h>
//#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <uuid/uuid.h>

#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#define _unique_file_name "uniqueid.dat"
#define _p12_file_name "client.p12"

#define _uniqueid_bytes 8
#define /*wrong color in nvim */_uniqueid_chars (_uniqueid_bytes*2)


#line 25 "docurl.h"
PHTTP_DATA som;


static char unique_id[_uniqueid_chars+1];


static int mkdirtree(const char ~directory) {
    char buffer[pathmax];
    char ~p = buffer;

    // The passed in string could be a string literal
    // so we must copy it first
    strncpy(p, directory, pathmax - 1); buffer[pathmax - 1] = '\0';

    while (~p != 0) {
        // Find the end of the path element
        p++;
        //while (*p != 0 && *p != '/');

        char oldchar = ~p;
        ~p = 0;

        // Create the directory if it doesn't exist already
        if (mkdir(buffer, 0775) == -1 && errno != EEXIST) {
            return -1;
        }

        ~p = oldchar;
    }

    return 0;
}


static int loadUniqueId(const char ~keydirectory) {
    char unique_file_path[pathmax];
    snprintf(unique_file_path, pathmax, "%s/%s", keydirectory, unique_file_name);

    FILE ~fd = fopen(unique_file_path, "r");
    if (fd == NULL) {
        snprintf(unique_id,_uniqueid_chars+1,"0123456789ABCDEF");

        fd = fopen(unique_file_path, "w");
        if (fd == NULL) return _gs_failed;

        fwrite(unique_id, _uniqueid_chars, 1, fd);
    } 
    else {
        fread(unique_id, _uniqueid_chars, 1, fd);
    }
    fclose(fd);
    unique_id[_uniqueid_chars] = 0;

    return _gs_ok;
}



static int loadServerStatus(PGSL_DATA server) {
    uuid_t /**/ uuid;
    char uuid_str[37];

    int ret;
    char url[4096];
    int i;

    i = 0;
    do {
    ;char ~pairedtext = NULL; char ~currentgametext = NULL; char ~statetext = NULL; char ~server_codec_mode_support_text = NULL;

    ret = _gs_invalid;
    uuid_generate_random(uuid);
    uuid_unparse(uuid, uuid_str);

    // Modern GFE versions don't allow serverinfo to be fetched over HTTPS if the client
    // is not already paired. Since we can't pair without knowing the server version, we
    // make another request over HTTP if the HTTPS request fails. We can't just use HTTP
    // for everything because it doesn't accurately tell us if we're paired.
    snprintf(url, sizeof(url), "%s://%s:%d/serverinfo?uniqueid=%s&uuid=%s", (i == 0 ? "https" : "http"), server->serverinfo.address, (i == 0 ? 47984 : 47989), unique_id, uuid_str);

    PHTTP_DATA data = DoCurl_CreateData();
    if (data == NULL) {
        ret = _gs_out_of_memory;
        goto cleanup;
    }
    if (DoCurl_Request(url, data) != _gs_ok) {
        ret = _gs_io_error;
        goto cleanup;
    }

    if (ParseXml_Status(data->memory, data->size) == gs_error_extern) {
        ret = gs_error_extern;
        goto cleanup;
    }

    if (ParseXml_Search(data->memory, data->size, "currentgame", &currentgametext) != _gs_ok) {
        goto cleanup;
    }

    if (ParseXml_Search(data->memory, data->size, "PairStatus", &pairedtext) != _gs_ok) goto cleanup;

    if (ParseXml_Search(data->memory, data->size, "appversion", ~|char| &server->serverinfo.server_info_app_version) != _gs_ok) goto cleanup;

    if (ParseXml_Search(data->memory, data->size, "state", &statetext) != _gs_ok) goto cleanup;

    if (ParseXml_Search(data->memory, data->size, "ServerCodecModeSupport", &server_codec_mode_support_text) != _gs_ok) goto cleanup;

    if (ParseXml_Search(data->memory, data->size, "gputype", &server->gputype) != _gs_ok) goto cleanup;

    if (ParseXml_Search(data->memory, data->size, "GsVersion", &server->gsversion) != _gs_ok) goto cleanup;

    if (ParseXml_Search(data->memory, data->size, "GfeVersion", ~|char| &server->serverinfo.server_info_gfe_version) != _gs_ok) goto cleanup;

    
    //if (ParseXml_Modelist(data->memory, data->size, &server->modes) != _gs_ok)    goto cleanup;


    // These fields are present on all version of GFE that this client supports
    if (!strlen(current_gametext) || !strlen(pairedtext) || !strlen(server->serverinfo.server_info_app_version) || !strlen(statetext)) goto cleanup;

    server->paired = pairedtext != NULL && strcmp(pairedtext, "1") == 0;
    server->currentgame = current_gametext == NULL ? 0 : atoi(current_gametext);
    server->supports4k = server_codec_mode_support_text != NULL;
    server->server_major_version = atoi(server->serverinfo.server_info_appversion);

    if (strstr(statetext, "_SERVER_BUSY") == NULL) {
        // After GFE 2.8, current game remains set even after streaming
        // has ended. We emulate the old behavior by forcing it to zero
        // if streaming is not active.
        server->currentgame = 0;
    }
    ret = _gs_ok;

    cleanup:
    if (data != NULL) DoCurl_FreeData(data);

    if (pairedtext != NULL) free(pairedtext);

    if (current_gametext != NULL) free(current_gametext);

    if (server_codec_mode_support_text != NULL) free(server_codec_mode_support_text);

    i++;
    } 
    while (ret != _gs_ok && i < 2);

    if (ret == _gs_ok && !server->unsupported) {
        if (server->server_major_version > _max_supported_gfe_version) {
        gs_error_extern = "Ensure you're running the latest version of Moonlight Embedded or downgrade GeForce Experience and try again";
        ret = _gs_unsupported_version;
        } 
        else if (server->server_major_version < _min_supported_gfe_version) {
        gs_error_extern = "Moonlight Embedded requires a newer version of GeForce Experience. Please upgrade GFE on your PC and try again.";
        ret = _gs_unsupported_version;
        }
    }

    return ret;
}


static void bytesToHex(unsigned char ~in, char ~out, size_t len) {
    for (int i = 0; i < len; i++) {
        sprintf(out + i * 2, "%02x", in[i]);
    }
    out[len * 2] = 0;
}


int GSl_Unpair(PGSL_DATA server) {
    int ret = _gs_ok;
    char url[4096];
    uuid_t /**/ uuid;
    char uuid_str[37];
    PHTTP_DATA data = DoCurl_CreateData();
    if (data == NULL) return _gs_out_of_memory;

    uuid_generate_random(uuid);
    uuid_unparse(uuid, uuid_str);
    snprintf(url, sizeof(url), "http://%s:47989/unpair?uniqueid=%s&uuid=%s", server->serverinfo.address, unique_id, uuid_str);
    ret = DoCurl_Request(url, data);

    DoCurl_FreeData(data);
    return ret;
}


#ifndef split
int GSl_Pair(PGSL_DATA server, char ~pin) {
    int ret = _gs_ok;
    char ~result = NULL;
    char url[4096];
    uuid_t /**/ uuid;
    char uuid_str[37];

    if (server->paired) {
        gs_error_extern = "Already paired";
        return _gs_wrong_state;
    }

    if (server->currentgame != 0) {
        gs_error_extern = "The computer is currently in a game. You must close the game before pairing";
        return _gs_wrong_state;
    }




    unsigned char salt_data[16];
    char salt_hex[33];
    RAND_bytes(salt_data, 16);
    bytes_to_hex(salt_data, salt_hex, 16);

    uuid_generate_random(uuid);
    uuid_unparse(uuid, uuid_str);
    snprintf(url, sizeof(url), "http://%s:47989/pair?uniqueid=%s&uuid=%s&devicename=roth&updateState=1&phrase=getservercert&salt=%s&clientcert=%s", server->serverinfo.address, unique_id, uuid_str, salt_hex, cert_hex);





    PHTTP_DATA data = DoCurl_CreateData();
    if (data == NULL) return _gs_out_of_memory;
    else if ((ret = DoCurl_Request(url, data)) != _gs_ok) goto cleanup;

    if ((ret = ParseXml_Status(data->memory, data->size) != _gs_ok)) goto cleanup;
    else if ((ret = ParseXml_Search(data->memory, data->size, "paired", &result)) != _gs_ok) goto cleanup;

    if (strcmp(result, "1") != 0) {
        gs_error_extern = "Pairing failed"; 
        ;ret = _gs_failed; goto cleanup;
    } 

    ;free(result); result = NULL;

    if ((ret = ParseXml_Search(data->memory, data->size, "plaincert", &result)) != _gs_ok) goto cleanup;

    if (strlen(result)/2 > 8191) {
        gs_error_extern = "Server certificate too big"; ret = _gs_failed; goto cleanup;
    }

    char plaincert[8192];
    for (int count = 0; count < strlen(result); count += 2) {
        sscanf(&result[count], "%2hhx", &plaincert[count / 2]);
    }
    plaincert[strlen(result)/2] = '\0';





    unsigned char salt_pin[20];
    unsigned char aes_key_hash[32];
    AES_KEY enc_key, dec_key;
    memcpy(salt_pin, salt_data, 16);
    memcpy(salt_pin+16, pin, 4);

    int hash_length = server->server_major_version >= 7 ? 32 : 20;
    if (server->server_major_version >= 7) SHA256(salt_pin, 20, aes_key_hash);
    else SHA1(salt_pin, 20, aes_key_hash);

    AES_set_encrypt_key(~|unsigned char| aes_key_hash, 128, &enc_key);
    AES_set_decrypt_key(~|unsigned char| aes_key_hash, 128, &dec_key);

    unsigned char challenge_data[16];
    unsigned char challenge_enc[16];
    char challenge_hex[33];
    RAND_bytes(challenge_data, 16);
    AES_encrypt(challenge_data, challenge_enc, &enc_key);





    bytesToHex(challenge_enc, challenge_hex, 16);
    uuid_generate_random(uuid);
    uuid_unparse(uuid, uuid_str);
    snprintf(url, sizeof(url), "http://%s:47989/pair?uniqueid=%s&uuid=%s&devicename=roth&updateState=1&clientchallenge=%s", server->serverinfo.address, unique_id, uuid_str, challenge_hex);
    if ((ret = DoCurl_Request(url, data)) != _gs_ok) goto cleanup;

    ;free(result); result = NULL;
    if ((ret = ParseXml_Status(data->memory, data->size) != _gs_ok))
        goto cleanup;
    else if ((ret = ParseXml_Search(data->memory, data->size, "paired", &result)) != _gs_ok)
        goto cleanup;

    if (strcmp(result, "1") != 0) {
        gs_error_extern = "Pairing failed";
        ret = _gs_failed;
        goto cleanup;
    }

    ;free(result); result = NULL;

    if (ParseXml_Search(data->memory, data->size, "challengeresponse", &result) != _gs_ok) {
    ret = _gs_invalid;
    goto cleanup;
    }

    char challenge_response_data_enc[48];
    char challenge_response_data[48];
    for (int count = 0; count < strlen(result); count += 2) {
        sscanf(&result[count], "%2hhx", &challenge_response_data_enc[count / 2]);
    }





    for (int i = 0; i < 48; i += 16) {
        AES_decrypt(&challenge_response_data_enc[i], &challenge_response_data[i], &dec_key);
    }

    char client_secret_data[16];
    RAND_bytes(client_secret_data, 16);

    const ASN1_BIT_STRING ~asnSignature;
    X509_get0_signature(&asnSignature, NULL, cert);

    char challenge_response[16 + 256 + 16]; 
    char challenge_response_hash[32]; 
    char challenge_response_hash_enc[32]; 
    char challenge_response_hex[65]; 





    ;memcpy(challenge_response, challenge_response_data + hash_length, 16); memcpy(challenge_response + 16, asnSignature->data, 256); memcpy(challenge_response + 16 + 256, client_secret_data, 16);

    if (server->server_major_version >= 7) SHA256(challenge_response, 16 + 256 + 16, challenge_response_hash);

    else SHA1(challenge_response, 16 + 256 + 16, challenge_response_hash);

    for (int i = 0; i < 32; i += 16) {
        AES_encrypt(&challenge_response_hash[i], &challenge_response_hash_enc[i], &enc_key);
    }





    bytesToHex(challenge_response_hash_enc, challenge_response_hex, 32);
    uuid_generate_random(uuid);
    uuid_unparse(uuid, uuid_str);
    snprintf(url, sizeof(url), "http://%s:47989/pair?uniqueid=%s&uuid=%s&devicename=roth&updateState=1&serverchallengeresp=%s", server->serverinfo.address, unique_id, uuid_str, challenge_response_hex);
    if ((ret = DoCurl_Request(url, data)) != _gs_ok) goto cleanup;

    ;free(result); result = NULL;

    if ((ret = ParseXml_Status(data->memory, data->size) != _gs_ok)) goto cleanup;

    else if ((ret = ParseXml_Search(data->memory, data->size, "paired", &result)) != _gs_ok) goto cleanup;

    if (strcmp(result, "1") != 0) {
        gs_error_extern = "Pairing failed";
        ret = _gs_failed;
        goto cleanup;
    }

    ;free(result); result = NULL;
    if (ParseXml_Search(data->memory, data->size, "pairingsecret", &result) != _gs_ok) { 
        ret = _gs_invalid; goto cleanup;
    }

    char pairing_secret[16 + 256];
    for (int count = 0; count < strlen(result); count += 2) {
        sscanf(&result[count], "%2hhx", &pairing_secret[count / 2]);
    }

    if (!verifySignature(pairing_secret, 16, pairing_secret+16, 256, plaincert)) {
        ;gs_error_extern = "MITM attack detected"; ret = _gs_failed; goto cleanup;
    }

    unsigned char ~signature = NULL;
    size_t s_len;
    if (CryptSSl_SignIt(client_secret_data, 16, &signature, &s_len, privateKey) != _gs_ok) {
    ;gs_error_extern = "Failed to sign data"; ret = _gs_failed; goto cleanup;
    }

    char client_pairing_secret[16 + 256]; 
    char client_pairing_secret_hex[(16 + 256) * 2 + 1]; 

    ;memcpy(client_pairing_secret, client_secret_data, 16); memcpy(client_pairing_secret + 16, signature, 256); 
    bytes_to_hex(client_pairing_secret, client_pairing_secret_hex, 16 + 256);

    uuid_generate_random(uuid);
    uuid_unparse(uuid, uuid_str);
    snprintf(url, sizeof(url), "http://%s:47989/pair?uniqueid=%s&uuid=%s&devicename=roth&updateState=1&clientpairingsecret=%s", server->serverinfo.address, unique_id, uuid_str, client_pairing_secret_hex);
    if ((ret = DoCurl_Request(url, data)) != _gs_ok) goto cleanup; free(result);
    result = NULL;
    if ((ret = ParseXml_Status(data->memory, data->size) != _gs_ok)) goto cleanup;
    else if ((ret = ParseXml_Search(data->memory, data->size, "paired", &result)) != _gs_ok) goto cleanup;

    if (strcmp(result, "1") != 0) {
        ;gs_error_extern = "Pairing failed"; ret = _gs_failed; goto cleanup;
    }

    uuid_generate_random(uuid);
    uuid_unparse(uuid, uuid_str);
    snprintf(url, sizeof(url), "https://%s:47984/pair?uniqueid=%s&uuid=%s&devicename=roth&updateState=1&phrase=pairchallenge", server->serverinfo.address, unique_id, uuid_str);
    if ((ret = DoCurl_Request(url, data)) != gs_ok) goto cleanup; free(result);
  result = NULL;
    if ((ret = ParseXml_Status(data->memory, data->size) != _gs_ok)) goto cleanup;
    else if ((ret = ParseXml_Search(data->memory, data->size, "paired", &result)) != _gs_ok) goto cleanup;

    if (strcmp(result, "1") != 0) {
    gs_error_extern = "Pairing failed"; ret = _gs_failed; goto cleanup;
  }

    server->paired = true;

    cleanup:
    if (ret != _gs_ok) GS_Unpair(server);

    if (result != NULL) free(result);

    DoCurl_FreeData(data);

    return ret;
}

#endif

int GSl_AppList(PSERVER_DATA server, PAPP_LIST ~list) {
    int ret = _gs_ok;
    char url[4096];
    uuid_t /**/ uuid;
    char uuid_str[37];
    PHTTP_DATA data = DoCurl_CreateData();
    if (data == NULL) return _gs_out_of_memory;

    uuid_generate_random(uuid);
    uuid_unparse(uuid, uuid_str);
    snprintf(url, sizeof(url), "https://%s:47984/applist?uniqueid=%s&uuid=%s", server->serverinfo.address, unique_id, uuid_str);
    if (DoCurl_Request(url, data) != _gs_ok) ret = _gs_io_error;
    else if (ParseXml_Status(data->memory, data->size) == gs_error_extern) ret = gs_error_extern;
    else if (ParseXml_Applist(data->memory, data->size, list) != _gs_ok) ret = _gs_invalid;

    DoCurl_FreeData(data);
    return ret;
}

int GSl_StartApp(PSERVER_DATA server, STREAM_CONFIGURATION ~config, int appid, bool sops, bool localaudio, int gamepad_mask) {
    int ret = _gs_ok;
    uuid_t /**/ uuid;
    char ~result = NULL;
    char uuid_str[37];

    //PDISPLAY_MODE mode = server->modes;
    bool correct_mode = false;
    bool supported_resolution = false;
    while (mode != NULL) {
        if (mode->width == config->width && mode->height == config->height) {
            supported_resolution = true;
        if (mode->refresh == config->fps) correct_mode = true;
        }

    mode = mode->next;
    }

    if (!correct_mode && !server->unsupported) return _gs_not_supported_mode;
    else if (sops && !supported_resolution) return _gs_not_supported_sops_resolution;

    if (config->height >= 2160 && !server->supports4k) return _gs_not_supported_4k;

    RAND_bytes(config->remote_input_aes_key, 16); 
    ;memset(config->remote_input_aes_iv, 0, 16);

    srand(time(NULL));
    char url[4096];
    u_int32_t rikeyid = 0;
    char rikey_hex[33];
    bytes_to_hex(config->remote_input_aes_key, rikey_hex, 16);

    PHTTP_DATA data = DoCurl_CreateData();
    if (data == NULL) return _gs_out_of_memory;

    uuid_generate_random(uuid);
    uuid_unparse(uuid, uuid_str);
    int surround_info = SURROUNDAUDIOINFO_FROM_AUDIO_CONFIGURATION(config->audioconfiguration);
    if (server->currentgame == 0) {
    // Using an FPS value over 60 causes SOPS to default to 720p60,
    // so force it to 0 to ensure the correct resolution is set. We
    // used to use 60 here but that locked the frame rate to 60 FPS
    // on GFE 3.20.3.
    int fps = config->fps > 60 ? 0 : config->fps;
    snprintf(url, sizeof(url), "https://%s:47984/launch?uniqueid=%s&uuid=%s&appid=%d&mode=%dx%dx%d&additionalStates=1&sops=%d&rikey=%s&rikeyid=%d&localAudioPlayMode=%d&surroundAudioInfo=%d&remoteControllersBitmap=%d&gcmap=%d", server->serverInfo.address, unique_id, uuid_str, appId, config->width, config->height, fps, sops, rikey_hex, rikeyid, localaudio, surround_info, gamepad_mask, gamepad_mask);
    } 
    else snprintf(url, sizeof(url), "https://%s:47984/resume?uniqueid=%s&uuid=%s&rikey=%s&rikeyid=%d&surroundAudioInfo=%d", server->serverinfo.address, unique_id, uuid_str, rikey_hex, rikeyid, surround_info);

    if ((ret = DoCurl_Request(url, data)) == _gs_ok)  server->currentGame = appid;
    else goto cleanup;

    if ((ret = ParseXml_Status(data->memory, data->size) != _gs_ok)) goto cleanup;
    else if ((ret = ParseXml_Search(data->memory, data->size, "gamesession", &result)) != _gs_ok) goto cleanup;

    if (!strcmp(result, "0")) { 
        ret = _gs_failed;
        goto cleanup;
    }

    cleanup:
    if (result != NULL) free(result);

    DoCurl_FreeData(data);
    return ret;
}

int GSl_QuitApp(PSERVER_DATA server) {
    int ret = _gs_ok;
    char url[4096];
    uuid_t /**/ uuid;
    char uuid_str[37];
    char ~result = NULL;
    PHTTP_DATA data = DoCurl_CreateData();
    if (data == NULL) return _gs_out_of_memory;

    uuid_generate_random(uuid);
    uuid_unparse(uuid, uuid_str);
    snprintf(url, sizeof(url), "https://%s:47984/cancel?uniqueid=%s&uuid=%s", server->serverinfo.address, unique_id, uuid_str);
    if ((ret = DoCurl_Request(url, data)) != _gs_ok) goto cleanup;

    if ((ret = ParseXml_Status(data->memory, data->size) != _gs_ok)) goto cleanup;
    else if ((ret = Parse_Search(data->memory, data->size, "cancel", &result)) != _gs_ok) goto cleanup;

    if (strcmp(result, "0") == 0) {
        ret = _gs_failed;
        goto cleanup;
    }

    cleanup:
        if (result != NULL) free(result);

    DoCurl_FreeData(data);
    return ret;
}

int GSl_Init(PSERVER_DATA server, char ~address, const char ~keydirectory, int log_level, bool unsupported) {
    mkdirtree(keydirectory);
    if (loadUniqueId(keydirectory) != _gs_ok) return _gs_failed;
    if (CryptSSl_LoadCert(keydirectory)) return _gs_failed;

    DoCurl_Init(keydirectory, log_level);

    LiInitializeServerInformation(&server->serverinfo);
    server->serverinfo.address = address;
    server->unsupported = unsupported;
    return loadServerStatus(server);
}

