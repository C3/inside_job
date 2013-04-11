require "bundler/gem_tasks"
require 'rake/testtask'
require 'rake/clean'
require 'rake/extensiontask'
require 'rspec/core/rake_task'

RSpec::Core::RakeTask.new(:spec)

Rake::ExtensionTask.new do |ext|
  ext.name           = 'inside_job_ext'
  ext.ext_dir        = 'ext/inside_job'
  ext.lib_dir        = 'lib'
  ext.source_pattern = "*.{c,h}"
end

task :spec => :compile

desc "Run tests"
task :default => :spec

desc "Run tests through valgrind"
task :valgrind => :compile do
  system "valgrind --partial-loads-ok=yes --undef-value-errors=no --dsymutil=yes --leak-check=full --show-reachable=yes --log-file=valgrind.log ruby -S rake spec"
end

desc "Run tests through gdb"
task :gdb => :compile do
  system "gdb --args `which ruby` spec/gdb.rb"
end
