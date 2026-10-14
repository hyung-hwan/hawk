package main

import "os"

func main() {
	var args []string
	var status int

	args = os.Args[1:]
	if len(args) > 0 && args[0] == "--httpd" {
		status = main_httpd(args[1:])
	} else {
		status = main_hawk(args)
	}
	os.Exit(status)
}
