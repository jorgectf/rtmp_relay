package server

import "log"

type Relay struct {
	servers []Server
}

func NewRelay(config Config) *Relay {
	relay := &Relay{}

	for _, serverConfig := range config.Servers {
		for _, endpointConfig := range serverConfig.Endpoints {
			log.Println(endpointConfig.ApplicationName)
		}
	}

	return relay
}

func (relay *Relay) Close() {

}

func (relay *Relay) Run() {

}
