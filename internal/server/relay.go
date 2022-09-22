package server

type Relay struct {
	servers []Server
}

func NewRelay(config Config) *Relay {
	relay := &Relay{}

	return relay
}

func (relay *Relay) Run() {

}
