#include "csapp.h"

int
main(int argc, char **argv)
{
	int		n;
	rio_t		rio;
	char		buf       [MAXLINE];

	Rio_readinitb(&rio, STDIN_FILENO);
	if (argc == 2)
		//If we have optional argument
	{
		int		fd = Open(argv[1], O_RDONLY, 0);
		//create fd for checking
			if reading
				inputfile succeeds
					if (fd < 0)
					//Read failed, exit
						exit(0);
		while ((n = Rio_readn(fd, buf, MAXLINE)) != 0)
			//copy the file to standard outpt
				Rio_writen(STDOUT_FILENO, buf, n);
	}
	while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
		Rio_writen(STDOUT_FILENO, buf, n);

	return 0;
}
