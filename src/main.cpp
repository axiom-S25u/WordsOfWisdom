#include <Geode/Geode.hpp>
#include <Geode/modify/LoadingLayer.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/coro.hpp>
#include <Geode/utils/base64.hpp>
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <map>

using namespace geode::prelude;

static LoadingLayer* s_ptr=nullptr;
static geode::async::TaskHolder<void> s_holder;

struct Info {
	std::string usr;
	std::string txt;
};

static std::vector<Info> pool;
// this is NOT the same thing as loadingscreentweaks, lst has custom ones YOU choose, this..well.. doesnt, its all random lol
std::vector<std::string> split(const std::string& str, char delim)
{
	std::vector<std::string> res;
	std::string temp;
	for (char c : str)
	{
		if (c==delim)
		{
			res.push_back(temp);
			temp.clear();
		}
		else
		{
			temp+=c;
		}
	}
	if (!temp.empty())
	{
		res.push_back(temp);
	}
	return res;
}

std::map<std::string, std::string> parseRobTop(const std::string& str, char splitter)
{
	std::map<std::string, std::string> res;
	std::vector<std::string> parts=split(str, splitter);
	for (size_t n=0; n+1<parts.size(); n+=2)
	{
		res[parts[n]]=parts[n+1];
	}
	return res;
}

std::string decBase64(std::string_view src)
{
	std::string s(src);
	for (auto& c : s)
	{
		if (c=='-') c='+';
		if (c=='_') c='/';
	}
	auto dec=geode::utils::base64::decodeString(s, geode::utils::base64::Base64Variant::Normal);
	if (dec.ok())
	{
		return dec.unwrap();
	}
	auto dec2=geode::utils::base64::decodeString(s, geode::utils::base64::Base64Variant::Url);
	if (dec2.ok())
	{
		return dec2.unwrap();
	}
	return std::string(src);
}

arc::Future<void> getWisdom()
{
	for (int attempts=0; attempts<10; attempts++)
	{
		int pg=std::rand()%351;
		web::WebRequest req;
		req.header("Content-Type", "application/x-www-form-urlencoded");
		req.userAgent("");
		req.bodyString("gameVersion=22&binaryVersion=47&gdw=0&type=1&page="+std::to_string(pg)+"&secret=Wmfd2893gb7");//boom,lings?
		web::WebResponse res=co_await req.post("http://www.boomlings.com/database/getGJLevels21.php"); 
		if (!res.ok()||res.code()==204)
		{
			continue;
		}
		std::string body=res.string().unwrapOr("");
		if (body.empty()||body=="-1"||body.starts_with("error"))
		{
			continue;
		}
		std::vector<std::string> parts=split(body, '#');
		if (parts.empty())
		{
			continue;
		}
		std::vector<std::string> lvls=split(parts[0], '|');
		if (lvls.empty())
		{
			continue;
		}
		std::string levelStr=lvls[std::rand()%lvls.size()];
		std::map<std::string, std::string> lvl=parseRobTop(levelStr, ':');
		std::string idStr=lvl["1"];
		if (idStr.empty())
		{
			continue;
		}
		
		int cpg=std::rand()%5;
		web::WebRequest req2;
		req2.header("Content-Type", "application/x-www-form-urlencoded");
		req2.userAgent("");
		req2.bodyString("gameVersion=22&binaryVersion=47&gdw=0&levelID="+idStr+"&page="+std::to_string(cpg)+"&count=10&mode=0&secret=Wmfd2893gb7");
		res=co_await req2.post("http://www.boomlings.com/database/getGJComments21.php");
		if (!res.ok()||res.code()==204)
		{
			continue;
		}
		std::string cbody=res.string().unwrapOr("");
		if (cbody.empty()||cbody=="-1"||cbody.starts_with("error"))
		{
			continue;
		}
		std::vector<std::string> cparts=split(cbody, '#');
		if (cparts.empty())
		{
			continue;
		}
		std::vector<std::string> comments=split(cparts[0], '|');
		for (int n=0; n<(int)comments.size(); n++)
		{
			std::vector<std::string> cinfo=split(comments[n], ':');
			if (cinfo.size()>=2)
			{
				std::map<std::string, std::string> commentData=parseRobTop(cinfo[0], '~');
				std::map<std::string, std::string> userData=parseRobTop(cinfo[1], '~');
				std::string rawTxt=commentData["2"];
				std::string username=userData["1"];
				if (!rawTxt.empty()&&!username.empty())
				{
					Info inf;
					inf.usr=username;
					inf.txt=decBase64(rawTxt);
					pool.push_back(inf);
				}
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
			line->setID("words-of-wisdom"_spr); // yet again, its not the same like loadingscreentweaks
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