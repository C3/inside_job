require 'rubygems'
require 'bundler'

Bundler.setup

version = ">= 0"

gem 'rake', version
load Gem.bin_path('rake', 'rake', version)

require 'rake'
Rake::Task['spec'].invoke