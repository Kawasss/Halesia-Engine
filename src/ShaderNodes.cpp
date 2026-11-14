module;

#include <imgui-1.91.7/imgui.h>
#include <imgui-1.91.7/misc/cpp/imgui_stdlib.h>

#include <imnodes/imnodes.h>

module Editor.ShaderNodes;

import Editor.UI;

namespace editor
{
	bool Link::Contains(int x) const
	{
		return a == x || b == x;
	}

	bool Link::operator==(const Link& other) const noexcept
	{
		return a == other.a && b == other.b;
	}

	void Link::Draw() const
	{
		ImNodes::Link(a, a, b);
	}

	CountedID::CountedID() : id(GetUniqueID()) {}

	int CountedID::GetUniqueID()
	{
		static int counter = std::numeric_limits<int>::min() + 1; // imnodes uses INT_MIN
		return counter++;
	}

	int CountedID::GetID() const
	{
		return id;
	}

	std::string CountedID::AsInvisibleUIName() const
	{
		return std::format("##{}", id);
	}

	FloatValue::FloatValue() : input("") {}
	FloatValue::FloatValue(const std::string_view& name) : input(name) {} // TODO: i dont know if only using the name will cause conflicts later

	float FloatValue::AsFloat() const
	{
		return input.GetValue();
	}

	int FloatValue::AsInt() const
	{
		return static_cast<int>(input.GetValue());
	}
	
	Port::Port(Type type, std::unique_ptr<Value>&& v) : type(type), value(std::move(v)) {}

	void PortContainer::AddPort(std::unique_ptr<Port>&& p)
	{
		switch (p->type)
		{
		case Port::Type::Input:
			ports.emplace(p->GetID(), std::move(p));
			break;
		case Port::Type::Output:
			ports.emplace(p->GetID(), std::move(p));
			break;
		}
	}

	void PortContainer::RemovePort(int id)
	{
		if (auto it = ports.find(id); it != ports.end())
			ports.erase(it);
	}

	OptionalRef<Port> PortContainer::FindPort(int id)
	{
		auto it = ports.find(id);

		return it == ports.end() ? OptionalRef<Port>() : *it->second;
	}

	void PortContainer::Draw()
	{
		for (const auto& [id, port] : ports)
			port->Draw();
	}

	void BasicNode::Draw()
	{
		ImNodes::BeginNode(id);

		ImNodes::PushColorStyle(ImNodesCol_::ImNodesCol_TitleBar, color);
		ImNodes::BeginNodeTitleBar();

		ImGui::Text(name.c_str());

		ImNodes::EndNodeTitleBar();
		ImNodes::PopColorStyle();

		inputs.Draw();
		outputs.Draw();

		ImNodes::EndNode();
	}

	BasicNode::BasicNode(ImVec2 size, ImU32 color, const std::string_view& name) : size(size), color(color), name(name) {}

	ConstFloatPort::ConstFloatPort() : Port(Type::Output, std::make_unique<FloatValue>()) {}
	ConstFloatPort::ConstFloatPort(const std::string_view& name) : Port(Type::Output, std::make_unique<FloatValue>(name)) {}

	void ConstFloatPort::Draw()
	{
		FloatValue& val = dynamic_cast<FloatValue&>(*value);

		ImNodes::BeginOutputAttribute(id);
		ImGui::PushID(id);
		ImGui::PushItemWidth(75);
		val.input.Update();
		ImGui::PopID();
		ImNodes::EndOutputAttribute();
	}

	EmptyPort::EmptyPort(const std::string_view& name) : Port(Type::Output, std::unique_ptr<Value>()), name(name) {}

	void EmptyPort::Draw()
	{
		ImNodes::BeginOutputAttribute(id);
		ImGui::Text(name.data());
		ImNodes::EndOutputAttribute();
	}

	ConstVec2Node::ConstVec2Node() : BasicNode(ImVec2(100, 50), IM_COL32(0, 255, 0, 255), "vec2")
	{
		outputs.AddPort(std::make_unique<EmptyPort>("xy"));
		outputs.AddPort(std::make_unique<ConstFloatPort>("x"));
		outputs.AddPort(std::make_unique<ConstFloatPort>("y"));
	}

	ConstFloatNode::ConstFloatNode() : BasicNode(ImVec2(100, 50), IM_COL32(0, 0, 255, 255), "float")
	{
		outputs.AddPort(std::make_unique<ConstFloatPort>());
	}

	FragmentOutputNode::FragmentOutputNode() : BasicNode(ImVec2(100, 50), IM_COL32(255, 0, 0, 255), "fragment out")
	{
		inputs.AddPort(std::make_unique<GenericInputPort>("x"));
		inputs.AddPort(std::make_unique<GenericInputPort>("y"));
		inputs.AddPort(std::make_unique<GenericInputPort>("z"));
		inputs.AddPort(std::make_unique<GenericInputPort>("w"));
	}

	GenericInputPort::GenericInputPort(const std::string_view& name) : Port(Type::Input, std::unique_ptr<Value>()), name(name) {}

	void GenericInputPort::Draw()
	{
		ImNodes::BeginInputAttribute(id);
		ImGui::Text(name.data());
		ImNodes::EndInputAttribute();
	}
}