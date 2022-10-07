package server

import (
	"context"
	"errors"
	"log"
	"net"
	"time"
)

type Connection struct {
	connectionType    string
	address           string
	connectionTimeout float32
	reconnectInterval float32
	reconnectCount    uint32
	buffer            []byte
}

func NewConnection(
	connectionType string,
	address string,
	connectionTimeout float32,
	reconnectInterval float32,
	reconnectCount uint32,
	bufferSize uint32) *Connection {
	connection := &Connection{
		connectionType:    connectionType,
		address:           address,
		connectionTimeout: connectionTimeout,
		reconnectInterval: reconnectInterval,
		reconnectCount:    reconnectCount,
		buffer:            make([]byte, bufferSize),
	}

	return connection
}

func (connection *Connection) Close() {

}

func (connection *Connection) Run(ctx context.Context) {

	if connection.connectionType == "host" {
		connection.listen(ctx)
	} else if connection.connectionType == "client" {
		connection.connect(ctx)
	}
}

func (connection *Connection) listen(ctx context.Context) {
	go func() {
		select {
		case <-ctx.Done():
			log.Println("Context done")
		}
	}()

	var listenConfig net.ListenConfig

	listener, err := listenConfig.Listen(ctx, "tcp", connection.address)
	if err != nil {
		log.Println("Failed to create server", err)
		// TODO: reconnect
		return
	}

	defer listener.Close()

	go func() {
		select {
		case <-ctx.Done():
			log.Println("Context done")
			if err := listener.Close(); err != nil {
				log.Println("Failed to close listener", err)
			}
		}
	}()

	conn, err := listener.Accept()
	if err != nil {
		if errors.Is(err, net.ErrClosed) {
			log.Println("Listener closed")
		} else {
			log.Println("Failed to accept client", err)
		}
		return
	}

	go func() {
		select {
		case <-ctx.Done():
			log.Println("Context done")
			if err := conn.Close(); err != nil {
				log.Println("Failed to close connection", err)
			}
		}
	}()

	connection.handleConnection(conn)
}

func (connection *Connection) connect(ctx context.Context) {
	ctx, _ = context.WithTimeout(ctx, time.Duration(connection.connectionTimeout*float32(time.Second)))

	var dialer net.Dialer
	conn, err := dialer.DialContext(ctx, "tcp", connection.address)
	if err != nil {
		if errors.Is(err, context.DeadlineExceeded) {
			log.Println("Context deadline exceeded")
			// TODO: reconnect
			return
		} else if errors.Is(err, context.Canceled) {
			log.Println("Context canceled")
			return
		} else {
			log.Println("Failed to connect to server", err)
			// TODO: reconnect
			return
		}
	}

	defer conn.Close()

	go func() {
		select {
		case <-ctx.Done():
			log.Println("Context done")
			if err := conn.Close(); err != nil {
				log.Println("Failed to close connection", err)
			}
		}
	}()

	connection.handleConnection(conn)
}

func (connection *Connection) handleConnection(conn net.Conn) {
	for {
		n, err := conn.Read(connection.buffer)
		if err != nil {
			if errors.Is(err, net.ErrClosed) {
				log.Println("Connection closed")
			} else {
				log.Println("Failed read data", err)
			}
			// TODO: reconnect
			return
		}

		log.Println("Received", n, "bytes")
	}
}
