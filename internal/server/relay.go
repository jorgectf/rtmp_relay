package server

import (
	"context"
	"sync"
)

type Relay struct {
	servers []*Server
}

func NewRelay(config Config) *Relay {
	relay := &Relay{
		servers: make([]*Server, len(config.Servers)),
	}

	for i, serverConfig := range config.Servers {
		server := NewServer(serverConfig)
		relay.servers[i] = server
	}

	return relay
}

func (relay *Relay) Close() {
	for _, server := range relay.servers {
		server.Close()
	}
}

func (relay *Relay) Run(ctx context.Context) {
	var wg sync.WaitGroup

	for _, server := range relay.servers {
		wg.Add(1)

		go func(server *Server) {
			defer wg.Done()
			server.Run(ctx)
		}(server)
	}

	wg.Wait()
}
