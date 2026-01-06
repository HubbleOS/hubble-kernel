#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define GDB_PORT 1234
#define BUF_SIZE 8192

unsigned char hex_to_byte(char hi, char lo)
{
	unsigned char b = 0;
	if (hi >= '0' && hi <= '9')
		b += (hi - '0') << 4;
	else if (hi >= 'a' && hi <= 'f')
		b += (hi - 'a' + 10) << 4;
	else if (hi >= 'A' && hi <= 'F')
		b += (hi - 'A' + 10) << 4;

	if (lo >= '0' && lo <= '9')
		b += (lo - '0');
	else if (lo >= 'a' && lo <= 'f')
		b += (lo - 'a' + 10);
	else if (lo >= 'A' && lo <= 'F')
		b += (lo - 'A' + 10);

	return b;
}

int gdb_send_packet(int sock, const char *cmd, char **out_data, size_t *out_len)
{
	char buf[BUF_SIZE];
	int n;

	unsigned char checksum = 0;
	for (size_t i = 0; i < strlen(cmd); i++)
		checksum += cmd[i];

	char packet[BUF_SIZE];
	snprintf(packet, sizeof(packet), "$%s#%02x", cmd, checksum);
	send(sock, packet, strlen(packet), 0);

	// Ждём ACK '+'
	n = recv(sock, buf, 1, 0);
	if (n <= 0 || buf[0] != '+')
	{
		printf("No ACK from GDB stub.\n");
		return 1;
	}

	if (out_data)
	{
		// Читаем данные до '#'
		size_t pos = 0;
		while (1)
		{
			n = recv(sock, buf + pos, BUF_SIZE - pos - 1, 0);
			if (n <= 0)
				break;
			pos += n;
			buf[pos] = '\0';
			if (strchr(buf, '#'))
				break;
		}

		char *hash = strchr(buf, '#');
		if (!hash)
			return 1;
		*hash = '\0';

		*out_len = strlen(buf) / 2;
		*out_data = malloc(*out_len);
		for (size_t i = 0; i < *out_len; i++)
		{
			(*out_data)[i] = hex_to_byte(buf[i * 2], buf[i * 2 + 1]);
		}
	}

	return 0;
}

void print_regs_x86_64(const unsigned char *data, size_t len)
{
	if (len < 27 * 8)
		return;

	const char *names[] = {
	    "RAX", "RBX", "RCX", "RDX", "RSI", "RDI", "RBP", "RSP",
	    "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15",
	    "RIP", "EFLAGS", "CS", "SS", "DS", "ES", "FS", "GS"};

	for (int i = 0; i < 24; i++)
	{
		unsigned long long val = 0;
		for (int b = 0; b < 8; b++)
			val |= ((unsigned long long)data[i * 8 + b]) << (b * 8);
		printf("%4s = 0x%016llx\n", names[i], val);
	}
}

void print_memory(const unsigned char *data, size_t len, unsigned long long addr)
{
	for (size_t i = 0; i < len; i += 16)
	{
		printf("%016llx: ", addr + i);
		for (size_t j = 0; j < 16 && i + j < len; j++)
			printf("%02x ", data[i + j]);
		for (size_t j = 16; j > len - i ? len - i : 0; j--)
			printf("   ");
		printf(" |");
		for (size_t j = 0; j < 16 && i + j < len; j++)
		{
			unsigned char c = data[i + j];
			printf("%c", (c >= 32 && c <= 126) ? c : '.');
		}
		printf("|\n");
	}
}

unsigned long long get_rip(int sock)
{
	char *reg_data = NULL;
	size_t reg_len = 0;
	unsigned long long rip = 0;
	if (gdb_send_packet(sock, "g", &reg_data, &reg_len) == 0)
	{
		for (int i = 0; i < 8; i++)
			rip |= ((unsigned long long)reg_data[16 * 8 + i]) << (i * 8);
		free(reg_data);
	}
	return rip;
}

int main()
{
	int sock;
	struct sockaddr_in server;

	printf("=== Mini Debug Interactive GDB v0.4 ===\n");

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		perror("socket");
		return 1;
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(GDB_PORT);
	server.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
		perror("connect");
		return 1;
	}

	printf("Connected to QEMU GDB stub at port %d\n", GDB_PORT);

	char line[256];
	while (1)
	{
		printf("dbg> ");
		if (!fgets(line, sizeof(line), stdin))
			break;
		if (strncmp(line, "q", 1) == 0)
			break;
		else if (strncmp(line, "regs", 4) == 0)
		{
			char *data = NULL;
			size_t len;
			if (gdb_send_packet(sock, "g", &data, &len) == 0)
				print_regs_x86_64((unsigned char *)data, len);
			free(data);
		}
		else if (strncmp(line, "mem", 3) == 0)
		{
			unsigned long long addr;
			int len;
			if (sscanf(line + 3, "%llx %d", &addr, &len) == 2)
			{
				char cmd[64];
				snprintf(cmd, sizeof(cmd), "m%llx,%d", addr, len);
				char *data = NULL;
				size_t dlen;
				if (gdb_send_packet(sock, cmd, &data, &dlen) == 0)
					print_memory((unsigned char *)data, dlen, addr);
				free(data);
			}
			else
				printf("Usage: mem <addr> <len>\n");
		}
		else if (strncmp(line, "s", 1) == 0)
			gdb_send_packet(sock, "s", NULL, NULL);
		else if (strncmp(line, "c", 1) == 0)
			gdb_send_packet(sock, "c", NULL, NULL);
		else
			printf("Unknown command\n");
	}

	close(sock);
	printf("Disconnected.\n");
	return 0;
}
