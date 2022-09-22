package server

type LogConfig struct {
	Level          uint32 `yaml:"level"`
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
	Video             *bool    `yaml:"video" default:"true"`
	Audio             *bool    `yaml:"audio" default:"true"`
	Data              *bool    `yaml:"data" default:"true"`
	MetaDataBlacklist []string `yaml:"metaDataBlacklist"`
	ConnectionTimeout *float32 `yaml:"connectionTimeout"`
	ReconnectInterval *float32 `yaml:"reconnectInterval"`
	ReconnectCount    *uint32  `yaml:"reconnectCount"`
	PingInterval      *float32 `yaml:"pingInterval"`
	BufferSize        *uint32  `yaml:"bufferSize"`
	AmfVersion        *uint32  `yaml:"amfVersion"`
}

type ServersConfig struct {
	Endpoints []EndpointConfig `yaml:"endpoints"`
}

type Config struct {
	Log        LogConfig        `yaml:"log"`
	StatusPage StatusPageConfig `yaml:"statusPage"`
	Servers    []ServersConfig  `yaml:"servers"`
}
