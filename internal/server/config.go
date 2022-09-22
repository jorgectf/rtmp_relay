package server

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
	Video             *bool
	Audio             *bool
	Data              *bool
	MetaDataBlacklist []string
	ConnectionTimeout *float32
	ReconnectInterval *float32
	ReconnectCount    *uint32
	PingInterval      *float32
	BufferSize        *uint32
	AmfVersion        *uint32
}

type ServersConfig struct {
	Endpoints []EndpointConfig
}

type Config struct {
	Log        LogConfig
	StatusPage StatusPageConfig
	Servers    []ServersConfig
}
