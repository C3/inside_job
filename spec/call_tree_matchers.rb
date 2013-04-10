RSpec::Matchers.define :produce do |expected|
  match do |actual|
    actual_names = actual.collect(&:name)
    expected_names = expected.collect(&:name)

    actual_names.should eql expected_names
  end

  failure_message_for_should do |actual|
    actual_tree = actual.print_tree
    expected_tree = expected.print_tree

    <<-EOF
observed call tree:
#{actual_tree}
didn't match expected call tree:
#{expected_tree}
    EOF
  end

  failure_message_for_should_not do |acutal|
    "the call trees shouldn't match, but they did"
  end
end

RSpec::Matchers.define :have_always_increasing_wall_clock_values do
  match do |actual|
    actual_wall_clock_values = actual.children.collect { |node| node.content.wall_start }
    actual_wall_clock_values.sort.should eql actual_wall_clock_values
  end

  failure_message_for_should do |actual|
    "expected #{actual} to be always increasing"
  end

  failure_message_for_should_not do |acutal|
    "expected #{actual} to not be always increasing"
  end
end

RSpec::Matchers.define :have_always_increasing_cpu_clock_values do
  match do |actual|
    actual_cpu_clock_values = actual.children.collect { |node| node.content.cpu_start }
    actual_cpu_clock_values.sort.should eql actual_cpu_clock_values
  end

  failure_message_for_should do |actual|
    "expected #{actual} to be always increasing"
  end

  failure_message_for_should_not do |acutal|
    "expected #{actual} to not be always increasing"
  end
end

# The default print_tree actually prints for you to STDOUT
# we want the actual tree ourselves for outputting
class Tree::TreeNode
  def print_tree(level = 0)
    tree = ""
    if is_root?
      tree << "*"
    else
      tree << "|" unless parent.is_last_sibling?
      tree << (' ' * (level - 1) * 4)
      tree << (is_last_sibling? ? "+" : "|")
      tree << "---"
      tree << (has_children? ? "+" : ">")
    end

    tree << " #{name}\n"

    tree << children.collect { |child| child.print_tree(level + 1)}.join("\n")

    tree
  end
end