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
			Reset,
			Continue,
		};

		void Transition(const std::shared_ptr<Scene>& next, EnterBehavior NextState)
		{
			// behavior not yet implemented

			auto it = std::find(paused.begin(), paused.end(), next);
			if (it == paused.end())
				currStates.push(next);
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
			return currStates.top();
		}

	private:
		std::stack<std::shared_ptr<Scene>> currStates;
		std::list<std::shared_ptr<Scene>> paused;
	};
}