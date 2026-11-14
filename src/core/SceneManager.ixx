export module Core.SceneManager;

import Core.Scene;

import std;

export namespace core
{
	class SceneManager
	{
	public:
		enum class ExitBehavior
		{
			Pause,
			Destroy,
		};

		enum class EnterBehavior
		{
			New,
			Reset,
			Continue,
		};

		enum class State
		{
			Paused,
			New,
		};

		struct ManagedScene
		{
			ManagedScene() = default;
			ManagedScene(State state, const std::shared_ptr<Scene>& scene) : state(state), scene(scene) {}

			State state;
			std::shared_ptr<Scene> scene;
		};

		void Cleanup()
		{
			paused.clear();
		}

		bool IsEmpty() const
		{
			return currStates.empty();
		}

		void Transition(const std::shared_ptr<Scene>& next, EnterBehavior nextState)
		{
			// behavior not yet implemented

			State state = State::New;

			switch (nextState)
			{
			case EnterBehavior::Reset:
				next->Destroy();
				next->Start();
				state = State::New;
				break;
			case EnterBehavior::New:
				next->Start();
				state = State::New;
				break;
			default:
				state = State::Paused;
				break;
			}

			auto it = std::find_if(paused.begin(), paused.end(), [&](const auto& scene) { return next == scene.scene; });
			if (it == paused.end())
				currStates.emplace(state, next);
			else
			{
				currStates.push(std::move(*it));
				paused.erase(it);
			}
		}

		void Pop(ExitBehavior behavior)
		{
			if (currStates.empty())
				return;

			switch (behavior)
			{
			case ExitBehavior::Destroy:
				currStates.pop();
				break;
			case ExitBehavior::Pause:
				paused.insert(paused.begin(), std::move(currStates.top()));
				currStates.pop();
				break;
			}
		}

		std::weak_ptr<Scene> GetCurrScene() const
		{
			return currStates.top().scene;
		}

	private:
		std::stack<ManagedScene> currStates;
		std::list<ManagedScene> paused;
	};
}