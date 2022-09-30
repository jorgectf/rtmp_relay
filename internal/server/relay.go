package server

import (
	"context"
	"sync"
)

type Relay struct {
	servers []*Server
	cancel  context.CancelFunc
}

func NewRelay(config Config) *Relay {
	ctx, cancel := context.WithCancel(context.Background())

	relay := &Relay{
		servers: make([]*Server, len(config.Servers)),
		cancel:  cancel,
	}

	for i, serverConfig := range config.Servers {
		server := NewServer(ctx, serverConfig)
		relay.servers[i] = server
	}

	return relay
}

func (relay *Relay) Close() {
	for _, server := range relay.servers {
		server.Close()
	}
}

func (relay *Relay) Run() {
	var wg sync.WaitGroup

	for _, server := range relay.servers {
		wg.Add(1)

		go func(server *Server) {
			defer wg.Done()
			server.Run()
		}(server)
	}

	wg.Wait()
}

func (relay *Relay) Stop() {
	relay.cancel()
}
