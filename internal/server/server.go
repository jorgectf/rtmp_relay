package server

import (
	"context"
	"sync"
)

type Server struct {
	endpoints []*Endpoint
}

func NewServer(config ServersConfig) *Server {
	server := &Server{
		endpoints: make([]*Endpoint, len(config.Endpoints)),
	}

	for i, endpointConfig := range config.Endpoints {
		endpoint := NewEndpoint(endpointConfig)
		server.endpoints[i] = endpoint
	}

	return server
}

func (server *Server) Close() {
	for _, endpoint := range server.endpoints {
		endpoint.Close()
	}
}

func (server *Server) Run(ctx context.Context) {
	var wg sync.WaitGroup

	for _, endpoint := range server.endpoints {
		wg.Add(1)

		go func(endpoint *Endpoint) {
			defer wg.Done()
			endpoint.Run(ctx)
		}(endpoint)
	}

	wg.Wait()
}
