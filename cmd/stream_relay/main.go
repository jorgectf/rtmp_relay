package main

import (
	"context"
	"flag"
	"github.com/elnormous/rtmp_relay/internal/server"
	"gopkg.in/yaml.v3"
	"log"
	"os"
	"os/signal"
	"syscall"
)

func main() {
	log.Println("RTMP relay started")
	defer log.Println("RTMP relay stopped")

	help := flag.Bool("help", false, "Show help")
	configPath := flag.String("config", "", "Path to config file")
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
	} else if len(*configPath) == 0 {
		log.Println("No config file given")
		return
	}

	if *daemon {
		log.Println("Not implemented")
		// TODO: run as daemon
		return
	}

	configFile, err := os.ReadFile(*configPath)
	if err != nil {
		log.Println("Failed to open config file,", err)
	}

	var config server.Config
	if configError := yaml.Unmarshal(configFile, &config); configError != nil {
		log.Println("Failed to parse config", configError)
	}

	relay := server.NewRelay(config)
	defer relay.Close()

	ctx, cancel := context.WithCancel(context.Background())

	go func(cancel context.CancelFunc) {
		signalChannel := make(chan os.Signal, 1)
		signal.Notify(signalChannel, syscall.SIGHUP, syscall.SIGINT, syscall.SIGTERM, syscall.SIGQUIT)
		s := <-signalChannel
		log.Println("Received a signal", s)
		cancel()
	}(cancel)

	relay.Run(ctx)
}
