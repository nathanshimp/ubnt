/**
 * @file ubnt.c
 * @author Nathan Shimp
 * @brief Data structures and methods to connect and communicate
 * with Ubiquit Network Devices
 */
#include "ubnt.h"

/**
 * Remove all whitespace from the right of a string
 *
 * @param str String to strip
 */
void rstrip(char *str)
{
    if (str)
    {
        int i = strlen(str);

        if (i > 0)
        {
            while (isspace(str[i - 1]))
                i--;

            str[i] = '\0';
        }
    }
}

/**
 * Remove all occurances of \\n, \\t and \\r from a string
 * and return the stripped string
 * @param str String to strip
 * @returns Stripped String
 */
char *strip(char *str)
{
    int length, j;
    char *stripped_str = NULL;

    if (str)
    {
        length = strlen(str) + 100;
        stripped_str = malloc(length);

        j = 0;
        for (int i = 0; i < length; i++)
            if (str[i] != '\n' && str[i] != '\t' && str[i] != '\r')
                stripped_str[j++] = str[i];

        stripped_str[j] = '\0';
    }

    return stripped_str;
}

/**
 * Convert port number to string
 *
 * @param port int representation to convert
 */
char *convert_port_to_string(int port)
{
    char *port_str = malloc(6);
    int rc = snprintf(port_str, 6, "%d", port);
    if (rc < 0)
        strcpy(port_str, "22");

    return port_str;
}

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

        ssh_options_set(device->session, SSH_OPTIONS_HOST, device->host);
        ssh_options_set(device->session, SSH_OPTIONS_PORT, &device->port);
        ssh_options_set(device->session, SSH_OPTIONS_USER, device->username);
    }


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
    rc = ssh_connect(device->session);
    rc = ssh_userauth_try_publickey(device->session, NULL, publickey);
    if (rc == SSH_AUTH_SUCCESS)
    {
        rc = ssh_pki_import_privkey_file(privatekey_file, NULL, NULL, NULL, &privatekey);
        rc = ssh_userauth_publickey(device->session, NULL, privatekey);
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

    rc = ssh_channel_open_session(channel);
    rc = ssh_channel_request_exec(channel, command);

    buffer = (char *) calloc(buffer_size, '0');
    while (ssh_channel_is_open(channel) && !ssh_channel_is_eof(channel))
    {
        pbytes = ssh_channel_poll_timeout(channel, TIMEOUT, 0);

        if (pbytes > 0)
            nbytes += ssh_channel_read_timeout(channel, buffer, pbytes, 0, TIMEOUT);
        else
            break; // Not proud of this // TODO Fix this
    }

    rstrip(buffer);

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);

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
    rc     = ubnt_connect_password(device, password);

    if (rc == 0)
    {
        output = ubnt_exec_command(device, command);
        ubnt_disconnect(device);
    }

    free(device);

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
    char *output = ubnt_exec_command(device, "wstalist");
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
    char *output = ubnt_exec_command(device, "iwlist ath0 scan | scanparser");
    char *parsed_output = NULL;

    if (output)
    {
        parsed_output = strip(output);
        free(output);
    }

    return parsed_output;
}

/**
 * Convert output of mca-status command into a usable JSON structure.
 * Don't question how it works...
 *
 * @param str Buffer to convert to JSON
 */
char *mca_to_json(char *str)
{
    int length = strlen(str);
    int mca_parsed_size = 2048;
    char *buffer = (char *) malloc(mca_parsed_size);

    char *sub = strstr(str, ", ");
    if (sub != NULL) 
        strncpy(sub, "--", 2);

    buffer[0] = '[';
    buffer[1] = '{';
    buffer[2] = '"';

    int i;
    int j;
    for (i = 0, j = 3; i < length; i++)
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
    char *output = ubnt_exec_command(device, "mca-status");
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
enum UBNT_STATUS ubnt_save(UBNTDevice *device)
{
    char *output = ubnt_exec_command(device, "cfgmtd -w -p /etc/");

    return output ? UBNT_SUCCESS : UBNT_ERROR;
}

/**
 * Check if Ubiquiti device is connected
 *
 * @param device UBNTDevice structure
 * @returns 0 for connected and 1 for disconnected
 */
int ubnt_is_connected(UBNTDevice *device)
{
    return !ssh_is_connected(device->session);
}

/**
 * Copy configuration file through scp to buffer
 *
 * @param device UBNTDevice structure
 * @param buffer char*
 *
 * Note: Ubiquiti busybox dropbear scp implementation only allows reads
 * of 2048 bytes
 *
 * @returns size of file copied or -1 on error
 */
int ubnt_copy_config_to_buffer(UBNTDevice *device, char *buffer)
{
    int rc;
    ssh_scp scp;

    scp = ssh_scp_new(device->session, SSH_SCP_READ, CONFIG);
    rc = ssh_scp_init(scp);
    if (rc == CONNECTION_OK)
    {
        rc = ssh_scp_pull_request(scp);
        if (rc == SSH_SCP_REQUEST_NEWFILE)
        {
            int i = 0;
            int file_size = ssh_scp_request_get_size(scp);
            int num_reads = (file_size / SCP_READ_SIZE) + 1;
            char tmp_buffer[num_reads][SCP_READ_SIZE];
            rc = 0; // reset rc for sum of bytes read

            ssh_scp_accept_request(scp);
            do
            {
                rc += ssh_scp_read(scp, tmp_buffer[i], file_size);
                i++;
            }
            while (ssh_scp_pull_request(scp) != SSH_SCP_REQUEST_EOF
                    && i < num_reads);

            memcpy(buffer, *tmp_buffer, file_size);
        }
    }

    ssh_scp_close(scp);
    ssh_scp_free(scp);

    return rc;
}
