module;

#include <imgui-1.91.7/imgui.h>

export module Editor.ShaderNodes;

import std;

import Editor.UI;

export namespace editor
{
	struct Link
	{
		int a, b;

		bool Contains(int x) const;

		bool operator==(const Link& other) const noexcept;

		void Draw() const;
	};

	class CountedID
	{
	public:
		CountedID();

		std::string AsInvisibleUIName() const;

		int GetID() const;

	protected:
		int id;

	private:
		static int GetUniqueID();
	};

	template<typename T>
	using OptionalRef = std::optional<std::reference_wrapper<T>>;
	
	class Value
	{
	public:
		Value() = default;
		virtual ~Value() = default;

		virtual float AsFloat() const = 0;
		virtual int AsInt() const = 0;
	};

	class FloatValue : public Value, CountedID
	{
	public:
		FloatValue();
		FloatValue(const std::string_view& name);

		float AsFloat() const override;
		int AsInt() const override;

		eui::InputField<float> input;
	};

	class Port : public CountedID
	{
	public:
		Port() = default;

		enum class Type
		{
			Input,
			Output,
		};

		virtual ~Port() = default;

		virtual void Draw() = 0;

		Type type;

		std::string name;

		std::unique_ptr<Value> value;

	protected:
		Port(Type type, std::unique_ptr<Value>&& v);
	};

	class PortContainer
	{
	public:
		void AddPort(std::unique_ptr<Port>&& p);

		void RemovePort(int id);

		OptionalRef<Port> FindPort(int id);

		void Draw();

	private:
		std::map<int, std::unique_ptr<Port>> ports;
	};

	class BasicNode : CountedID
	{
	public:
		virtual ~BasicNode() = default;

		ImVec2 position;
		ImVec2 size;
		ImU32 color;

		PortContainer inputs;
		PortContainer outputs;

		virtual void Draw();

	protected:
		BasicNode(ImVec2 size, ImU32 color, const std::string_view& name);

		std::string name;
	};

	class GenericInputPort : public Port
	{
	public:
		GenericInputPort(const std::string_view& name);

		void Draw() override;

	private:
		std::string_view name;
	};

	class ConstFloatPort : public Port
	{
	public:
		ConstFloatPort();
		ConstFloatPort(const std::string_view& name);

		void Draw() override;
	};

	class EmptyPort : public Port
	{
	public:
		EmptyPort(const std::string_view& name);

		void Draw() override;

	private:
		std::string_view name;
	};

	class ConstVec2Node : public BasicNode
	{
	public:
		ConstVec2Node();
	};

	class ConstFloatNode : public BasicNode
	{
	public:
		ConstFloatNode();
	};

	class FragmentOutputNode : public BasicNode
	{
	public:
		FragmentOutputNode();
	};
}

template<>
struct std::hash<editor::Link>
{
	std::size_t operator()(const editor::Link& link) const noexcept
	{
		std::uint32_t low = std::bit_cast<std::uint32_t>(std::min(link.a, link.b));
		std::uint32_t high = std::bit_cast<std::uint32_t>(std::max(link.a, link.b));

		return (static_cast<std::size_t>(high) << 32) | low;
	}
};