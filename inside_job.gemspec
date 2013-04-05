# coding: utf-8
lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'inside_job/version'

Gem::Specification.new do |spec|
  spec.name          = "inside_job"
  spec.version       = InsideJob::VERSION
  spec.authors       = ["Lucas Maxwell"]
  spec.email         = ["lucas@thecowsays.mu"]
  spec.description   = %q{}
  spec.summary       = %q{}
  spec.homepage      = ""
  spec.license       = "MIT"

  spec.files         = `git ls-files`.split($/)
  spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
  spec.test_files    = spec.files.grep(%r{^(test|spec|features)/})
  spec.require_paths = ["lib"]

  spec.add_development_dependency "bundler", "~> 1.3.0"
  spec.add_development_dependency "rake", "~> 10.0.0"
  spec.add_development_dependency "rspec", "~> 2.12.0"
  spec.add_development_dependency "rake-compiler", "~> 0.8.0"
  spec.add_development_dependency "pry"
end
