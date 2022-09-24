package server

import "context"

type Endpoint struct {
	applicationName string
	connections     []*Connection
}

func NewEndpoint(ctx context.Context, config EndpointConfig) *Endpoint {
	endpoint := &Endpoint{
		applicationName: config.ApplicationName,
	}

	endpoint.connections = make([]*Connection, len(config.Addresses))

	for i, address := range config.Addresses {
		endpoint.connections[i] = NewConnection(
			ctx,
			config.Type,
			address,
			config.ConnectionTimeout,
			config.ReconnectInterval,
			config.ReconnectCount)
	}

	return endpoint
}

func (endpoint *Endpoint) Close() {
	for _, connection := range endpoint.connections {
		connection.Close()
	}
}

func (endpoint *Endpoint) Run() {
	for _, connection := range endpoint.connections {
		connection.Run()
	}
}
