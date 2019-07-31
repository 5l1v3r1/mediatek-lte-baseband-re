#include <stddef.h>
#include <stdint.h>

volatile uint32_t * const mem = (volatile uint32_t * const)(0x00000000);
#define SRAM 0x00100000
#define L2_SRAM 0x00200000

static uint32_t UART_RBR;
static uint32_t UART_THR;
static uint32_t UART_LSR;

#define UART_LSR_DR   (1 << 0)
#define UART_LSR_THRE (1 << 5)

#define MAX_CMD_LEN 100
#define MAX_ARGS 3

static uint32_t readw(uint32_t reg) {
	return mem[reg/4];
}

static void writew(uint32_t reg, uint32_t word) {
	mem[reg/4] = word;
}

static char getchar(void) {
	char ret;

	// Wait for the UART to assert the "data ready" flag.
	while ((readw(UART_LSR) & UART_LSR_DR) == 0);

	ret = readw(UART_RBR) & 0xff;

	return ret;
}

static void putchar(char c) {
	// Wait for the UART to become ready.
	while ((readw(UART_LSR) & UART_LSR_THRE) == 0);

	if (c == '\n')
		putchar('\r');

	writew(UART_THR, c);
}

static size_t strnlen(const char * buf, size_t max_len) {
	size_t len = 0;
	for (size_t i = 0; i < max_len; i++) {
		if (buf[i] == 0)
			break;
		len++;
	}
	return len;
}

static void print(const char * buf) {
	for (size_t i = 0; ; i++) {
		if (buf[i] == 0)
			break;
		putchar(buf[i]);
	}
}

static void println(const char * buf) {
	print(buf);
	putchar('\n');
}

static int strcmp (const char * str1, const char * str2) {
	int ret = 0;
	for (size_t i = 0; ; i++) {
		ret = str1[i] - str2[i];
		if (str1[i] == 0 || str2[i] == 0 || ret != 0) {
			break;
		}
	}
	return ret;
}

static int strncmp (const char * str1, const char * str2, size_t num) {
	int ret = 0;
	for (size_t i = 0; i < num; i++) {
		ret = str1[i] - str2[i];
		if (str1[i] == 0 || str2[i] == 0 || ret != 0) {
			break;
		}
	}
	return ret;
}

void * memset (void * ptr, int value, size_t num) {
	for (size_t i = 0; i < num; i++) {
		((uint8_t *)ptr)[i] = (uint8_t)value;
	}
}

static void init(void) {
	uint32_t UART_BASE;
	uint32_t soc_id = readw(0x08000000);
	switch(soc_id) {
	case 0x0279:
		// MT6797
		UART_BASE = 0x11002000;
		break;
	case 0x0321:
		// MT6735
		UART_BASE = 0x11002000;
		break;
	case 0x0335:
		// MT6737M
		UART_BASE = 0x11002000;
		break;
	case 0x8163:
		// MT8163
		UART_BASE = 0x11002000;
		break;
	default:
		UART_BASE = 0x11002000;
		break;
	}
	UART_RBR = UART_BASE + 0x00;
	UART_THR = UART_BASE + 0x00;
	UART_LSR = UART_BASE + 0x14;
}

static int parse_hex(uint32_t * value, const uint8_t * str) {
	size_t start = 0;
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
		start = 2;
	}

	size_t len = strnlen(&str[start], 18-start);
	if (len > 8) {
		println("Error: Hex string too long.");
		return -1;
	}

	*value = 0;
	for (size_t i = start; i < len + start; i++) {
		uint8_t nybble = 0;
		if ('a' <= str[i] && str[i] <= 'f') {
			nybble = str[i] - 'a' + 0xa;
		} else if ('A' <= str[i] && str[i] <= 'F') {
			nybble = str[i] - 'A' + 0xa;
		} else if ('0' <= str[i] && str[i] <= '9') {
			nybble = str[i] - '0';
		} else {
			print("Error: Bad character in hex string: ");
			putchar(str[i]);
			putchar('\n');
			return -1;
		}

		*value |= nybble << (4 * (len + start - i - 1));
	}

	return 0;
}

static void print_value(size_t value, size_t digits) {
	putchar('0');
	putchar('x');
	for (size_t i = 0; i < digits; i++) {
		uint8_t nybble = (value >> (4 * (digits - i - 1))) & 0xf;
		uint8_t chr = 0;
		if (0 <= nybble && nybble <= 9) {
			chr = nybble + '0';
		} else if (0xa <= nybble && nybble <= 0xf) {
			chr = nybble - 0xa + 'a';
		} else {
			chr = '?';
		}
		putchar(chr);
	}
}

static int mrw_handler(size_t argc, const char * argv[]) {
	if (argc < 2) {
		println("Error: Too few arguments.");
		println("Usage: mrw address");
		println("Examples:");
		println("    mrw 0x00000000");
		println("    mrw 0x8");
		println("    mrw 00000");
		println("    mrw c");
		println("    mrw 00201000");
		println("    mrw 201000");
		return -1;
	}

	uint32_t ptr = 0;
	int ret = parse_hex(&ptr, argv[1]);
	if (ret != 0) {
		println("Error: parse_hex() failed.");
		return -1;
	}
	if (ptr % 4) {
		println("Error: address must be 4-byte aligned.");
		return -1;
	}
	print_value(ptr, 8);
	print(": ");

	uint32_t value = readw(ptr);
	print_value(value, 8);
	putchar('\n');

	return 0;
}

static int mww_handler(size_t argc, const char * argv[]) {
	if (argc < 3) {
		println("Error: Too few arguments.");
		println("Usage: mww address value");
		println("Examples:");
		println("    mww 0x00200000 0");
		println("    mww 0x100008 1234");
		println("    mww 100000 0x008");
		println("    mww 20100c 0x1");
		println("    mww 00201000 0");
		return -1;
	}

	int ret = 0;

	uint32_t ptr = 0;
	ret = parse_hex(&ptr, argv[1]);
	if (ret != 0) {
		println("Error: parse_hex(argv[1]) failed.");
		return -1;
	}
	if (ptr % 4) {
		println("Error: address must be 4-byte aligned.");
		return -1;
	}
	print_value(ptr, 8);
	print(": ");
	print_value(readw(ptr), 8);
	putchar('\n');

	uint32_t value = 0;
	ret = parse_hex(&value, argv[2]);
	if (ret != 0) {
		println("Error: parse_hex(argv[2]) failed.");
		return -1;
	}
	writew(ptr, value);

	print_value(ptr, 8);
	print(": ");
	print_value(readw(ptr), 8);
	putchar('\n');

	return ret;
}

static int help_handler(size_t argc, const char * argv[]);

typedef struct {
	char * command;
	int (* handler)(size_t, const char **);
} command;

static const command cmd_table[] = {
	{ "help", help_handler },
	{ "mrw", mrw_handler },
	{ "mww", mww_handler },
	{ 0, 0 },
};

static int help_handler(size_t argc, const char * argv[]) {
	println("Commands available:");
	for (size_t i = 0; cmd_table[i].command != 0; i++) {
		print(" - ");
		println(cmd_table[i].command);
	}
	return 0;
}

static int handle_command(size_t argc, const char * argv[]) {
	if (argc > 0) {
		for (size_t i = 0; cmd_table[i].command != 0; i++) {
			if (!strncmp(argv[0], cmd_table[i].command, MAX_CMD_LEN)) {
				return cmd_table[i].handler(argc, argv);
			}
		}
		print("Error: Unknown command: ");
		println(argv[0]);
	}

	return -1;
}

static int parse_cmdline(char * buf) {
	size_t argc = 0;
	const char * argv[MAX_ARGS] = { 0 };

	for (size_t i = 0; i < MAX_CMD_LEN + 1 && argc != MAX_ARGS; i++) {
		switch (buf[i]) {
		case 0:
		case ' ':
		case '\t':
			if (argv[argc] != 0) {
				buf[i] = 0;
				argc++;
			}
			break;
		default:
			if (argv[argc] == 0) {
				argv[argc] = &buf[i];
			}
			break;
		}
	}

	return handle_command(argc, argv);
}

static void cmdloop(void) {
	while (1) {
		print("> ");
		char cmd_buf[MAX_CMD_LEN + 1] = { 0 };
		size_t cmd_len = 0;
		int cmd_entered = 0;
		while (!cmd_entered) {
			char c = getchar();
			switch (c) {
			case 0x1b:
				// Escape sequences.
				c = getchar();
				switch (c) {
				case '[':
					c = getchar();
					switch (c) {
					case 'A':
					case 'B':
					case 'C':
					case 'D':
						// Arrow keys.
						break;
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
						// Paging.
						getchar();
						break;
					default:
						print("^[[");
						putchar(c);
					}
					break;
				case 'O':
					c = getchar();
					switch (c) {
					case 'F':
						// End key.
						break;
					default:
						print("^[O");
						putchar(c);
					}
					break;
				default:
					print("^[");
					putchar(c);
				}
				break;
			case '\r':
				// Newline.
				cmd_buf[cmd_len] = 0;
				cmd_entered = 1;
				break;
			case '\b':
				// Backspace.
				if (cmd_len > 0) {
					print("\b \b");
					cmd_buf[--cmd_len] = 0;
				}
				break;
			default:
				if (cmd_len < MAX_CMD_LEN) {
					putchar(c);
					cmd_buf[cmd_len++] = c;
				}
			}
		}
		putchar('\n');
		parse_cmdline(cmd_buf);
	}
}

void main(void) {
	init();
	println("Hello from AArch64!");
	cmdloop();
}