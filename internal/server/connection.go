package server

import (
	"context"
	"log"
	"net"
)

type Connection struct {
	ctx context.Context

	connectionType    string
	address           string
	connectionTimeout float32
	reconnectInterval float32
	reconnectCount    uint32

	listener net.Listener
	conn     net.Conn
}

func NewConnection(
	ctx context.Context,
	connectionType string,
	address string,
	connectionTimeout float32,
	reconnectInterval float32,
	reconnectCount uint32) *Connection {
	connection := &Connection{
		ctx:               ctx,
		connectionType:    connectionType,
		address:           address,
		connectionTimeout: connectionTimeout,
		reconnectInterval: reconnectInterval,
		reconnectCount:    reconnectCount,
	}

	return connection
}

func (connection *Connection) Close() {

}

func (connection *Connection) Run() {
	if connection.connectionType == "host" {
		listenConfig := net.ListenConfig{}

		listener, err := listenConfig.Listen(connection.ctx, "tcp", connection.address)
		if err != nil {
			log.Println("Failed to create server", err)
		}

		connection.listener = listener
	} else if connection.connectionType == "client" {
		var dialer net.Dialer
		conn, err := dialer.DialContext(connection.ctx, "tcp", connection.address)
		if err != nil {
			log.Println("Failed to connect to server", err)
		}

		connection.conn = conn
	}

	log.Println("Done")
}
