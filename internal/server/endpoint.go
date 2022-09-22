package server

type Endpoint struct {
	applicationName string
}

func NewEndpoint(config EndpointConfig) *Endpoint {
	endpoint := &Endpoint{
		applicationName: config.ApplicationName,
	}

	return endpoint
}

func (endpoint *Endpoint) Close() {
}

func (endpoint *Endpoint) Run() {
}
