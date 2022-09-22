package server

import "gopkg.in/yaml.v3"

type LogConfig struct {
	Level          uint32
	SyslogIdent    string
	SyslogFacility string
}

type StatusPageConfig struct {
	Address string
}

type EndpointConfig struct {
	ApplicationName   string
	StreamName        string
	Type              string
	Direction         string
	Addresses         []string
	Video             bool
	Audio             bool
	Data              bool
	MetaDataBlacklist []string
	ConnectionTimeout float32
	ReconnectInterval float32
	ReconnectCount    uint32
	PingInterval      float32
	BufferSize        uint32
	AmfVersion        uint32
}

func (endpointConfig *EndpointConfig) UnmarshalYAML(value *yaml.Node) error {
	endpointConfig.Video = true
	endpointConfig.Audio = true
	endpointConfig.Data = true
	endpointConfig.ConnectionTimeout = 5.0
	endpointConfig.ReconnectInterval = 5.0
	endpointConfig.PingInterval = 60.0
	endpointConfig.BufferSize = 3000

	type tempEndpointConfig EndpointConfig
	return value.Decode((*tempEndpointConfig)(endpointConfig))
}

type ServersConfig struct {
	Endpoints []EndpointConfig
}

type Config struct {
	Log        LogConfig
	StatusPage StatusPageConfig
	Servers    []ServersConfig
}
