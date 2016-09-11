/**
 * @file
 * @author Nathan Shimp
 * @brief Data structures and methods to connect and communicate
 * with Ubiquit Network Devices
 */
#ifndef UBNT_DEVICE_H
#define UBNT_DEVICE_H
#include <stdlib.h>
#include <libssh/libssh.h>
#include "utils.h"
#define CONNECTION_OK 0
#define CONNECTION_ERROR -1
#define CONNECTION_AGAIN -2
#define CONNECTION_EOF -127
#define TIMEOUT 30000

/**
 * Structure to contain device address, credentials and session (connection)
 */
typedef struct UBNTDevice
{
    char *host;
    int port;
    char *username;
    char *password;
    ssh_session session;
} UBNTDevice;

/**
 * Initialize and return a new device
 *
 * @param host IP Address or Hostname
 * @param port Port to establish connection
 * @param username User to authenticate with
 * @param password Password to authenticate user with
 */
UBNTDevice *ubnt_init(char *host, int port, char *username)
{
    UBNTDevice *device = NULL;
    device = malloc(sizeof(UBNTDevice)); // 40 bytes

    if (device)
    {
        device->host = host;
        device->port = port;
        device->username = username;
        device->session = ssh_new();
    }

    ssh_options_set(device->session, SSH_OPTIONS_HOST, device->host);
    ssh_options_set(device->session, SSH_OPTIONS_PORT, &device->port);
    ssh_options_set(device->session, SSH_OPTIONS_USER, device->username);

    return device;
}

/**
 * Re-creates device session (connection)
 *
 * @param device UBNTDevice structure
 */
void ubnt_renew_session(UBNTDevice *device)
{
    device->session = ssh_new();
    ssh_options_set(device->session, SSH_OPTIONS_HOST, device->host);
    ssh_options_set(device->session, SSH_OPTIONS_PORT, &device->port);
    ssh_options_set(device->session, SSH_OPTIONS_USER, device->username);
}

/**
 * Initialize device session (connection) using password authentication
 *
 * @param device UBNTDevice structure
 * @returns 0 if succesful otherwise connection failed
 */
int ubnt_connect_password(UBNTDevice *device, char *password)
{
    int rc;

    rc = ssh_connect(device->session);
    if (rc == SSH_OK)
    {
        rc = ssh_userauth_password(device->session, NULL, password);
    }

    return rc;
}

/**
 * Initialize device session (connection) using public key authentication
 *
 * @param device UBNTDevice structure
 * @returns 0 if succesful otherwise connection failed
 */
int ubnt_connect_key(UBNTDevice *device, const char *publickey_file, const char *privatekey_file)
{
    int rc;
    ssh_key publickey = ssh_key_new();
    ssh_key privatekey = ssh_key_new();

    rc = ssh_pki_import_pubkey_file(publickey_file, &publickey);
    if (rc == SSH_OK)
    {
        rc = ssh_connect(device->session);
        if (rc == SSH_OK)
        {
            rc = ssh_userauth_try_publickey(device->session, NULL, publickey);
            if (rc == SSH_AUTH_SUCCESS)
            {
                rc = ssh_pki_import_privkey_file(privatekey_file, NULL, NULL, NULL, &privatekey);
                if (rc == SSH_OK)
                {
                    rc = ssh_userauth_publickey(device->session, NULL, privatekey);
                }
            }
        }
    }

    ssh_key_free(publickey);
    ssh_key_free(privatekey);
    return rc;
}

/**
 * Disconnect device and free all related memory
 *
 * @param device UBNTDevice structure
 */
void ubnt_disconnect(UBNTDevice *device)
{
    ssh_disconnect(device->session);
    ssh_free(device->session);
}

/**
 * Execute remote command on specified device
 *
 * @param device UBNTDevice structure
 * @param command Remote command to execute
 */
char *ubnt_exec_command(UBNTDevice *device, char *command)
{
    int rc;
    int nbytes = 0;
    int pbytes = 0;
    int buffer_size = 8192;
    char *buffer = NULL;
    ssh_channel channel = ssh_channel_new(device->session);

    if (channel)
    {
        rc = ssh_channel_open_session(channel);
        if (rc == SSH_OK)
        {
            rc = ssh_channel_request_exec(channel, command);
            if (rc == SSH_OK)
            {
                buffer = calloc(buffer_size, '0');
                while (ssh_channel_is_open(channel) && !ssh_channel_is_eof(channel))
                {
                    pbytes = ssh_channel_poll_timeout(channel, TIMEOUT, 0);

                    if (pbytes > 0)
                    {
                        nbytes += ssh_channel_read_timeout(channel, buffer, pbytes, 0, TIMEOUT);
                    }
                    else
                    {
                        break; // Not proud of this
                    }
                }

                if (buffer)
                {
                    rstrip(buffer);
                }
            }
        }

        ssh_channel_send_eof(channel);
        ssh_channel_close(channel);
        ssh_channel_free(channel);
    }

    return buffer;
}

/**
 * Initialize, connect, execute remote command and disconnect all in one shot
 *
 * @param host IP address or hostname of device
 * @param port Port to establish connection on
 * @param username User to authenticate with
 * @param password Password to authenticate user with
 * @param command Remote command to execute
 */
char *ubnt_dispatch(char *host, int port, char *username, char *password, char *command)
{
    int rc;
    UBNTDevice *device = NULL;
    char *output = NULL;

    device = ubnt_init(host, port, username);

    if (device)
    {
        rc = ubnt_connect_password(device, password);

        if (rc == 0)
        {
            output = ubnt_exec_command(device, command);
            ubnt_disconnect(device);
        }

        free(device);
    }

    return output;
}

/**
 * List active connections from device
 * Device must have an active connection
 *
 * @param device UBNTDevice structure
 */
char *ubnt_wstalist(UBNTDevice *device)
{
    char *command = "wstalist";
    char *output = ubnt_exec_command(device, command);
    char *parsed_output = NULL;

    if (output)
    {
    	parsed_output = strip(output);
        free(output);
    }

    return parsed_output;
}

/**
 * Retrieve all other access points that device can wirelessly "see"
 *
 * @param device UBNTDevice structure
 */
char *ubnt_scan(UBNTDevice *device)
{
    char *command = "iwlist ath0 scan | scanparser";
    char *output = ubnt_exec_command(device, command);
    char *parsed_output = NULL;

    if (output)
    {
    	parsed_output = strip(output);
    	free(output);
    }

    return parsed_output;
}

/**
 * Convert output of mca-status command into a usable JSON structure
 *
 * @param str Buffer to convert to JSON
 */
char *mca_to_json(char *str)
{
    char *sub;
    int length = strlen(str);
    int mca_parsed_size = 2048;
    char *buffer = malloc(mca_parsed_size);

    sub = strstr(str, ", ");
    if (sub != NULL)
	strncpy(sub, "--", 2);

    buffer[0] = '[';
    buffer[1] = '{';
    buffer[2] = '"';
    int j = 3;

    for (int i = 0; i < length; i++)
    {
        switch (str[i])
        {
            case '=':
                buffer[j] = '"';
                buffer[++j] = ':';
                buffer[++j] = '"';
                break;
            case ',':
                buffer[j] = '"';
                buffer[++j] = ',';
                buffer[++j] = '"';
                break;
            case '\r':
                if (str[i - 1] == '\n')
                {
                    i++;
                    j--;
                }
                else
                {
                    buffer[j] = '"';
                }
            break;
            case '\n':
                buffer[j] = ',';
                buffer[++j] = '"';
                break;
            default:
                buffer[j] = str[i];
        }

        j++;
    }

    buffer[j] = '"';
    buffer[++j] = '}';
    buffer[++j] = ']';
    buffer[++j] = '\0';

    return buffer;
}

/**
 * Retrieve the output of the mca-status command
 *
 * @param device UBNTDevice structure
 */
char *ubnt_mca_status(UBNTDevice *device)
{
    char *command = "mca-status";
    char *output = ubnt_exec_command(device, command);
    char *parsed_output = NULL;

    if (output)
    {
        parsed_output = mca_to_json(output);
        free(output);
    }

    return parsed_output;
}

/**
 * Save configuration changes made on Ubiquiti device
 *
 * @param device UBNTDevice structure
 * @returns 1 on success and 0 on failure
 */
int ubnt_save(UBNTDevice *device)
{
    char *command = "cfgmtd -w -p /etc/";
    char *output = ubnt_exec_command(device, command);

    // TODO Bad check, only accounts for output...doesn't make sure it's the correct output
    return output ? 1 : 0;
}

/**
 * Check if Ubiquiti device is connected
 *
 * @param device UBNTDevice structure
 * @returns 0 for connected and 1 for disconnected
 */
int ubnt_is_connected(UBNTDevice *device)
{
    // TODO make this not suck
    return !ssh_is_connected(device->session);
}
#endif
