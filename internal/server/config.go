package server

import "gopkg.in/yaml.v3"

type LogConfig struct {
	Level          uint32 `yaml:"applicationName"`
	SyslogIdent    string `yaml:"syslogIdent"`
	SyslogFacility string `yaml:"syslogFacility"`
}

type StatusPageConfig struct {
	Address string `yaml:"address"`
}

type EndpointConfig struct {
	ApplicationName   string   `yaml:"applicationName"`
	StreamName        string   `yaml:"streamName"`
	Type              string   `yaml:"type"`
	Direction         string   `yaml:"direction"`
	Addresses         []string `yaml:"addresses"`
	Video             bool     `yaml:"video"`
	Audio             bool     `yaml:"audio"`
	Data              bool     `yaml:"data"`
	MetaDataBlacklist []string `yaml:"metaDataBlacklist"`
	ConnectionTimeout float32  `yaml:"connectionTimeout"`
	ReconnectInterval float32  `yaml:"reconnectInterval"`
	ReconnectCount    uint32   `yaml:"reconnectCount"`
	PingInterval      float32  `yaml:"pingInterval"`
	BufferSize        uint32   `yaml:"bufferSize"`
	AmfVersion        uint32   `yaml:"amfVersion"`
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
	Endpoints []EndpointConfig `yaml:"endpoints"`
}

type Config struct {
	Log        LogConfig        `yaml:"log"`
	StatusPage StatusPageConfig `yaml:"statusPage"`
	Servers    []ServersConfig  `yaml:"servers"`
}
