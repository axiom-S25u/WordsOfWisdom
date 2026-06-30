#include <Geode/Geode.hpp>
#include <Geode/modify/LoadingLayer.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/coro.hpp>
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>

using namespace geode::prelude;

static LoadingLayer* s_ptr=nullptr;
static geode::async::TaskHolder<void> s_holder;

struct Info {
	std::string usr;
	std::string txt;
};

static std::vector<Info> pool;

arc::Future<void> getWisdom()
{
	for (int attempts=0; attempts<10; attempts++)
	{
		int pg=std::rand()%351;
		web::WebRequest req;
		web::WebResponse res=co_await req.get("https://gdbrowser.com/api/search/*?page="+std::to_string(pg)+"&type=mostdownloaded");
		if (!res.ok()||res.code()==204)
		{
			continue;
		}
		auto jsonRes=res.json();
		if (!jsonRes.ok()||!jsonRes.unwrap().isArray()||jsonRes.unwrap().size()==0)
		{
			continue;
		}
		matjson::Value lvl=jsonRes.unwrap()[std::rand()%jsonRes.unwrap().size()];
		geode::Result<std::string> idRes=lvl["id"].asString();
		if (!idRes.ok())
		{
			continue;
		}
		std::string idStr=idRes.unwrap();
		int cpg=std::rand()%5;
		res=co_await req.get("https://gdbrowser.com/api/comments/"+idStr+"?page="+std::to_string(cpg));
		if (!res.ok()||res.code()==204)
		{
			continue;
		}
		auto cjsonRes=res.json();
		if (!cjsonRes.ok()||!cjsonRes.unwrap().isArray()||cjsonRes.unwrap().size()==0)
		{
			continue;
		}
		matjson::Value root=cjsonRes.unwrap();
		for (int n=0; n<(int)root.size(); n++)
		{
			matjson::Value item=root[n];
			geode::Result<std::string> contentRes=item["content"].asString();
			geode::Result<std::string> userRes=item["username"].asString();
			if (contentRes.ok()&&userRes.ok())
			{
				Info inf;
				inf.usr=userRes.unwrap();
				inf.txt=contentRes.unwrap();
				pool.push_back(inf);
			}
		}
		if (!pool.empty())
		{
			if (pool.size()>50)
			{
				pool.erase(pool.begin(), pool.begin()+(pool.size()-50));
			}
			Info inf=pool[std::rand()%pool.size()];
			std::string str="Words of Wisdom: "+inf.usr+": "+inf.txt;
			Loader::get()->queueInMainThread([str] {
				if (s_ptr!=nullptr)
				{
					if (CCLabelBMFont* lbl=typeinfo_cast<CCLabelBMFont*>(s_ptr->getChildByID("words-of-wisdom"_spr)))
					{
						lbl->setString(str.c_str());
						float scl=s_ptr->m_textArea?s_ptr->m_textArea->getScale():1.0f;
						lbl->limitLabelWidth(420.f, scl, .25f);
					}
				}
			});
			co_return;
		}
	}
	co_return;
}

class $modify(MyLoadingLayer, LoadingLayer) {
	bool init(bool refresh)
	{
		if (!LoadingLayer::init(refresh))
		{
			return false;
		}
		s_ptr=this;
		CCNode* textArea=m_textArea?m_textArea:getChildByID("text-area");
		if (textArea!=nullptr)
		{
			textArea->setVisible(false);
			std::string str;
			if (!pool.empty())
			{
				Info inf=pool[std::rand()%pool.size()];
				str="Words of Wisdom: "+inf.usr+": "+inf.txt;
			}
			else
			{
				str=getLoadingString();
			}
			CCLabelBMFont* line=CCLabelBMFont::create(str.c_str(), "goldFont.fnt");
			line->setPosition(textArea->getPosition());
			line->limitLabelWidth(420.f, textArea->getScale(), .25f);
			line->setID("words-of-wisdom"_spr);
			this->addChild(line);
		}
		s_holder.spawn(getWisdom(), []() {});
		return true;
	}

	void onExit()
	{
		LoadingLayer::onExit();
		if (s_ptr==this)
		{
			s_ptr=nullptr;
			s_holder.cancel();
		}
	}
};