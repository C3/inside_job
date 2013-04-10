require 'rubytree'
require 'ostruct'

module CallTreeHelpers

  def trace_for(&block)
    output_file = Tempfile.new('inside_job')

    InsideJob.start(output_file.path)
    yield
    InsideJob.stop

    # the trace will contain the return from InsideJob.start and the call
    # from InsideJob.stop, the line/file of InsideJob.start isn't consistent
    # seems to be (null):0 for the first run, trace_helpers.rb:8 after?!!?
    # delete them for now, they don't add much
    lines = output_file.readlines[1..-2]

    line_format = /(return|call): (.+):(\d+) (.+) (\w+) (.*) (.*)/

    tree = CallTree.new.root_node

    parent = tree
    old_parent = nil
    lines.each do |line|
      line =~ line_format

      if $1 == 'call'
        call_node = MethodCall.build(parent, $4, $5, File.basename($2), $3.to_i)
        call_node.content.wall_start = $6.to_f
        call_node.content.cpu_start = $7.to_f

        old_parent = parent
        parent = call_node
      else
        call_node = parent.content
        call_node.wall_end = $6.to_f
        call_node.cpu_end = $7.to_f

        parent = old_parent
        old_parent = call_node.parent
      end
    end

    tree
  end

  def call_tree(&block)
    call_tree = CallTree.new
    call_tree.instance_eval(&block)
    call_tree.root_node
  end

end

# TODO: ahahahaha :(
class CallTree

  attr_reader :root_node

  def initialize
    @root_node = Tree::TreeNode.new("root")
  end

  def method_call(klass, method, file, line, &block)
    MethodCall.build(@root_node, klass, method, file, line, &block)
  end

end

class MethodCall < Struct.new(:parent, :klass, :method, :file, :line, :wall_start, :wall_end, :cpu_start, :cpu_end)

  def self.build(parent, klass, method, file, line, &block)
    call_node = Tree::TreeNode.new("#{file}:#{line} #{klass} #{method}")
    method_call = MethodCall.new(call_node, klass, method, file, line)
    call_node.content = method_call
    parent << call_node

    method_call.instance_eval(&block) if block_given?

    call_node
  end

  def method_call(klass, method, file, line, &block)
    MethodCall.build(parent, klass, method, file, line, &block)
  end

end
