package main

import (
	"flag"
	"log"
)

func main() {
	log.Println("RTMP relay started")
	defer log.Println("RTMP relay stopped")

	help := flag.Bool("help", false, "Show help")
	configFile := flag.String("config", "", "Path to config file")
	daemon := flag.Bool("daemon", false, "Run RTMP relay as daemon")
	killDaemon := flag.Bool("kill-daemon", false, "Kill the daemon")
	reloadConfig := flag.Bool("realod-config", false, "Print the documentation")

	flag.Parse()

	if *help {
		flag.Usage()
		return
	}

	if *killDaemon {
		log.Println("Not implemented")
		// TODO: kill the daemon
		return
	} else if *reloadConfig {
		log.Println("Not implemented")
		// TODO: reload the config
		return
	} else if len(*configFile) == 0 {
		log.Println("No config file given")
		return
	}

	if *daemon {
		log.Println("Not implemented")
		// TODO: run as daemon
		return
	}

	log.Println(*configFile)
}
