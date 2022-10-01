package server

import (
	"context"
	"sync"
)

type Endpoint struct {
	applicationName string
	connections     []*Connection
}

func NewEndpoint(ctx context.Context, config EndpointConfig) *Endpoint {
	endpoint := &Endpoint{
		applicationName: config.ApplicationName,
		connections:     make([]*Connection, len(config.Addresses)),
	}

	for i, address := range config.Addresses {
		endpoint.connections[i] = NewConnection(
			ctx,
			config.Type,
			address,
			config.ConnectionTimeout,
			config.ReconnectInterval,
			config.ReconnectCount,
			config.BufferSize)
	}

	return endpoint
}

func (endpoint *Endpoint) Close() {
	for _, connection := range endpoint.connections {
		connection.Close()
	}
}

func (endpoint *Endpoint) Run() {
	var wg sync.WaitGroup

	for _, connection := range endpoint.connections {
		wg.Add(1)

		go func(connection *Connection) {
			defer wg.Done()
			connection.Run()
		}(connection)
	}

	wg.Wait()
}
