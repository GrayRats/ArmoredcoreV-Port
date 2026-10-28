#include <cassert>
#include <cstdint>
#include <map>
#include <memory>
#include <iostream>
#define REXCVAR_GET(name) policy.name
#define REXLOG_INFO(...) ((void)0)
#define COUNT_profile_set(...) ((void)0)
#define assert_true(v) assert(v)
struct Policy {uint32_t texture_cache_memory_limit_render_to_texture=24,
 texture_cache_memory_limit_soft=384,texture_cache_memory_limit_hard=768,
 texture_cache_memory_limit_soft_lifetime=30;} policy;
namespace rex::chrono {struct Clock {static uint64_t QueryHostUptimeMillis();};}
static uint64_t now=10001;
uint64_t rex::chrono::Clock::QueryHostUptimeMillis(){return now;}
class TextureCache {
 public:
  struct Texture {
    TextureCache* owner;Texture* next=nullptr;uint32_t id;uint64_t bytes,submission,time;
    uint32_t key() const{return id;}
    uint64_t last_usage_submission_index() const{return submission;}
    uint64_t last_usage_time() const{return time;}
    ~Texture(){owner->textures_total_host_memory_usage_-=bytes;if(owner->texture_used_first_==this)owner->texture_used_first_=next;}
  };
  std::map<uint32_t,std::unique_ptr<Texture>> textures_;
  Texture* texture_used_first_=nullptr,*last_=nullptr;
  uint64_t textures_total_host_memory_usage_=0;unsigned resets=0;
  uint32_t draw_resolution_scale_x()const{return 1;}
  uint32_t draw_resolution_scale_y()const{return 1;}
  void ResetTextureBindings(){++resets;}
  void CompletedSubmissionUpdated(uint64_t);
  void Fill(unsigned count,uint64_t submission=1) {
    for(unsigned i=0;i<count;++i){
      auto texture=std::make_unique<Texture>();texture->owner=this;texture->id=i;
      texture->bytes=64u<<20;texture->submission=submission;texture->time=10000;
      if(last_)last_->next=texture.get();else texture_used_first_=texture.get();
      last_=texture.get();textures_total_host_memory_usage_+=texture->bytes;textures_.emplace(i,std::move(texture));
    }
  }
};
#include "actual_texture_eviction.inc"
int main(){
  {
    TextureCache cache;cache.Fill(20);
    cache.CompletedSubmissionUpdated(1);
    assert(cache.textures_.size()==12 && cache.textures_total_host_memory_usage_==(768u<<20) && cache.resets==1);
  }
  policy.texture_cache_memory_limit_soft=1024;policy.texture_cache_memory_limit_hard=1536;
  policy.texture_cache_memory_limit_soft_lifetime=180;
  {
    TextureCache cache;cache.Fill(20);cache.CompletedSubmissionUpdated(1);
    assert(cache.textures_.size()==20 && cache.resets==0);
    now=40001;cache.CompletedSubmissionUpdated(1);assert(cache.textures_.size()==20);
    now=190001;cache.CompletedSubmissionUpdated(1);
    assert(cache.textures_.size()==16 && cache.textures_total_host_memory_usage_==(1024u<<20) && cache.resets==1);
  }
  now=10001;
  {
    TextureCache cache;cache.Fill(32,9);cache.CompletedSubmissionUpdated(8);
    assert(cache.textures_.size()==32 && cache.resets==0); // In-flight data never evicted, even over hard limit.
    cache.CompletedSubmissionUpdated(9);
    assert(cache.textures_.size()==24 && cache.textures_total_host_memory_usage_==(1536u<<20) && cache.resets==1);
  }
  std::cout<<"PASS actual texture eviction: longer bounded retention, soft/hard limits, single binding reset, no destruction of in-flight textures\n";
}
