/* 
 *  Printing and such.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include "check.h"

char *escape(char *s)
{
	char *c;
	char *tmp = strdup(s);
	for (c = tmp; *c; c++)
		*c = isprint(*c) ? *c : '.';
	return tmp;
}

static void expand_custom(char ch, check_context_t *ctx)
{
	char *s;
	if (!ctx)
		return;

	switch(ch)
	{
	case '$':
		fputc('$', stdout);
		break;
	case 'I':
		printf("%llx", ctx->current_inode->head.self);
		break;
	case 'F':
		s = escape(ctx->current_inode->name);
		printf("%s", s);
		free(s);
		break;
	case 'H':
		printf("%d", ctx->hash);
		break;
	}
}

void sad_print(char *fmt, check_context_t *ctx)
{
	int escape = 0;
	char ch;
	while ((ch = *fmt++) != 0)
	{
		if (escape)
		{
			expand_custom(ch, ctx);
			escape = 0;
		}
		else
		{
			escape = (ch == '$');
			if (!escape)
				fputc(ch, stdout);
		}
	}
	fputc('\n', stdout);
}

int prompt_yesno(char *msg)
{
	struct termios saveios, newios;
	char ch;

	tcgetattr(0, &saveios);
	memcpy(&newios, &saveios, sizeof(struct termios));
	newios.c_lflag &= ~(ECHO | ICANON);
	newios.c_cc[VTIME] = 0;
	newios.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &newios);

	printf("%s [y/N]", msg);
	ch = getchar();

	tcsetattr(0, TCSANOW, &saveios);

	fputc('\n', stdout);
	return tolower(ch) == 'y';
}
