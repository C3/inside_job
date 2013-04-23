# InsideJob

TODO: Write a gem description

## System Requirements

libzmq: 3.2.2+
libmsgpack: 0.5.4+

### Ubuntu

NOTE: Ubuntu currently only has up to 2.1.11 so use a PPA for now

    sudo apt-get install build-essential python-software-properties
    sudo add-apt-repository ppa:chris-lea/zeromq
    sudo apt-get install build-essential libzmq-dev libmsgpack-dev

### OS X

#### Ports

    port install zmq msgpack

#### Brew

    brew install zmq msgpack

## Installation

Add this line to your application's Gemfile:

    gem 'inside_job'

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install inside_job

## Usage

TODO: Write usage instructions here

## Contributing

1. Fork it
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create new Pull Request
