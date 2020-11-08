function ensure (a, b, desc, line)
{
	if (a != b) 
	{
		printf ("FAILURE[%d] %s\n", line, desc);
		exit (-1);
	}
}
