require 'rubytree'
require 'ostruct'

module CallTreeHelpers

  def trace_for(&block)
    output_file = Tempfile.new('inside_job')

    InsideJob.start(output_file.path)
    yield
    InsideJob.stop

    # the trace will contain the return from InsideJob.start and the call
    # from InsideJob.stop, they'll always be there and we don't want to
    # have to assert them
    lines = output_file.readlines[1..-2]

    line_format = /(return|call): (.+) (\w+) (.*) (.*)/

    tree = CallTree.new.root_node

    parent = tree
    old_parent = nil
    lines.each do |line|
      line =~ line_format

      if $1 == 'call'
        call_node = MethodCall.build(parent, $2, $3)
        call_node.content.wall_start = $4.to_f
        call_node.content.cpu_start = $5.to_f

        old_parent = parent
        parent = call_node
      else
        call_node = parent.content
        call_node.wall_end = $4.to_f
        call_node.cpu_end = $5.to_f

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

  def method_call(klass, method, &block)
    MethodCall.build(@root_node, klass, method, &block)
  end

end

class MethodCall < Struct.new(:parent, :klass, :method, :wall_start, :wall_end, :cpu_start, :cpu_end)

  def self.build(parent, klass, method, &block)
    call_node = Tree::TreeNode.new("#{klass} #{method}")
    method_call = MethodCall.new(call_node, klass, method)
    call_node.content = method_call
    parent << call_node

    method_call.instance_eval(&block) if block_given?

    call_node
  end

  def method_call(klass, method, &block)
    MethodCall.build(parent, klass, method, &block)
  end

end
