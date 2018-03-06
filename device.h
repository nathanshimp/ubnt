/**
 * @file device.h
 * @author Nathan Shimp
 * @brief Data structures and methods to connect and communicate
 * with Ubiquit Network Devices
 */
#ifndef UBNT_DEVICE_H
#define UBNT_DEVICE_H
#include <stdlib.h>
#include <string.h>
#include <libssh/libssh.h>
#include "utils.h"
#define CONNECTION_OK 0
#define CONNECTION_ERROR -1
#define CONNECTION_AGAIN -2
#define CONNECTION_EOF -127
#define TIMEOUT 30000
#define CONFIG "/tmp/system.cfg"
#define SCP_READ_SIZE 2048

/**
 * Structure to contain device address, credentials and session (connection)
 */
typedef struct UBNTDevice
{
    char *host;
    int port;
    char *username;
    ssh_session session;
} UBNTDevice;

/**
 * UBNT return codes
 */
enum UBNT_STATUS {UBNT_SUCCESS, UBNT_ERROR};

UBNTDevice *ubnt_init(char *host, int port, char *username);
void ubnt_renew_session(UBNTDevice *device);
int ubnt_connect_password(UBNTDevice *device, char *password);
int ubnt_connect_key(UBNTDevice *device,
		const char *publickey_file, const char *privatekey_file);
void ubnt_disconnect(UBNTDevice *device);
char *ubnt_exec_command(UBNTDevice *device, char *command);
char *ubnt_dispatch(char *host, int port, char *username, char *password, char *command);
char *ubnt_wstalist(UBNTDevice *device);
char *ubnt_scan(UBNTDevice *device);
char *mca_to_json(char *str);
char *ubnt_mca_status(UBNTDevice *device);
enum UBNT_STATUS ubnt_save(UBNTDevice *device);
int ubnt_is_connected(UBNTDevice *device);
int ubnt_copy_config_to_buffer(UBNTDevice *device, char *buffer);

#endif
