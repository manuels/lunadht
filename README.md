# LunaDHT

Your general purpose distributed-hash-table (DHT) accessible via DBus.

## Installation

    git clone https://github.com/manuels/lunadht.git
    cd lunadht
    git submodule update --init --recursive
    cd libcage
    omake .
    make
    cd ..
    make
    
    XDG_DATA_DIRS=. ./lunadhtd
    

## Security

LunaDHT is a young software project and it might be vulnerable to attacks.
Despite potential software bugs, distributed hash tables suffer from these flaws:

- Anyone can read what you put into the DHT
- An attacker might give manipulated values stored for a key
- An attacker might not give any values stored for a key, although you stored a value for it

## Demo

Start the LunaDHT daemon

    $ XDG_DATA_DIRS=. ./lunadhtd &

Then store something in the DHT
    
    # Usage: ./demo_put.py <app_id> <key>
    $ ./demo_put.py 4321 "What is the answer to the great question, of life, the universe and everything?" "42"

and try to get it again

    # Usage: ./demo_put.py <app_id> <key>
    $ ./demo_get.py 4321 "What is the answer to the great question, of life, the universe and everything?"
    42

## DBus API

Bus name: `org.manuel.LunaDHT`

Path: `org/manuel/LunaDHT`

Interface: `org.manuel.LunaDHT`

### Methods

You can choose your `app_id` freely but make sure you use the same value for it on all machines. Its purpose is to minimize conflicting key-value pairs between applications.

    put(uint32 api_id, char* key, char* value, int64 ttl)
Store `value` under `key` with a time-to-live of `ttl` in seconds.

    get(uint32 api_id, char* key) -> array of char* values
Get `values` stored under `key`
    
    join(char *host, uint16 port)
Join the DHT network using the node at `host:port`

## Bootstrapping Nodes
Currently I am running a single bootstrapping node (see nodes.gschema.xml for IPv4/v6 addresses). It is used by default so you don't have to call `join` for it.
If you would like to support the LunaDHT network by running your own bootstrapping node, drop me a mail.
