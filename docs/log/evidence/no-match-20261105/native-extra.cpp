#define main unused_ownership_main
#include "ownership.cpp"
#undef main
#include <cstring>

int main(int argc, char** argv) try {
    require(argc == 4, "fixture, existing text fixture, output directory required");
    {
    vd::Runtime runtime(1);
    vd::Runtime::Attachment attachment(runtime);
    vd::Runtime::Managed managed(runtime);
    void *value=nullptr, *keys=nullptr, *replacements=nullptr, *replacement=nullptr, *output=nullptr;
    constexpr std::size_t zero=0;
    vd::Runtime::Slot slots[]={{&value,sizeof(void*),&zero,1},{&keys,sizeof(void*),&zero,1},
        {&replacements,sizeof(void*),&zero,1},{&replacement,sizeof(void*),&zero,1},{&output,sizeof(void*),&zero,1}};
    vd::Runtime::Frame frame(runtime,slots,5);
    const std::string schema_key="void.array.element.v1/"+std::to_string(vd::ir::Type::string)+":";
    vd::NativeArrayField field{"",0,0,12,0};
    vd::NativeArrayElement element{schema_key.c_str(),sizeof(void*),alignof(void*),1,&field};
    auto setup=[&](int count, std::string_view text) {
        runtime.string_into(&value,text);
        runtime.string_into(&output,"sentinel");
        runtime.string_into(&replacement,"R");
        runtime.list_into(&keys,vd::ScalarEncoding::i32,count);
        auto layout=runtime.native_array_layout(element,count);
        runtime.array_new(&replacements,layout,true);
        for(int i=0;i<count;++i) {
            runtime.list_write(runtime.root_word(&keys),vd::ScalarEncoding::i32,i,1000+i);
            runtime.array_write(runtime.root_word(&replacements),layout,true,i,&replacement,true);
        }
        return layout;
    };
    auto invoke=[&](void** out,std::int64_t budget,vd::RuntimeFailure& failure) {
        return vd_text_translate(out,&value,&keys,&replacements,&element,budget,&failure);
    };
    for(int count : {0,1,81,256,257}) {
        auto layout=setup(count,"ordinary message");
        vd::RuntimeFailure failure;
        auto original=value;
        require(invoke(&output,16,failure)==0 && !failure.message && output==value,"no-match did not publish original identity");
        require(invoke(&value,16,failure)==0 && !failure.message && value==original,"aliased no-match changed identity");
        require(invoke(&value,15,failure)==3 && !failure.message && value==original,"no-match budget changed alias");
        require(invoke(&value,-1,failure)==1 && !failure.message && value==original,"negative budget changed alias");
        if(count) {
            runtime.list_write(runtime.root_word(&keys),vd::ScalarEncoding::i32,count-1,0xd800);
            require(invoke(&value,-1,failure)==2 && !failure.message && value==original,"invalid key lost precedence");
            runtime.list_write(runtime.root_word(&keys),vd::ScalarEncoding::i32,count-1,1000+count-1);
        }
        unsigned rejected=0;
        for(int n=0;n<2048;++n) {
            output=nullptr; failure={}; int status;
            { managed_test::AllocationFailure injected(n,true);status=invoke(&output,16,failure); }
            if(!failure.message) {
                require(status==0 && output==value,"allocation sweep did not publish original alias");
                break;
            }
            ++rejected;
            require(output==nullptr && value==original,"allocation failure published or changed alias");
            require(runtime.collect()==vd::Runtime::Collection::completed,"allocation failure leaked root");
            require(n<2047,"allocation sweep did not succeed");
        }
        std::cout<<"no-match rules="<<count<<" allocation failures="<<rejected<<'\n';
    }
    auto layout=setup(81,"ordinary message");
    auto original=value;vd::RuntimeFailure failure;
    void* unrooted=nullptr;
    invoke(&unrooted,16,failure);require(failure.message && !unrooted,"unrooted no-match output accepted");
    failure={};void* unrooted_input=value;
    vd_text_translate(&value,&unrooted_input,&keys,&replacements,&element,16,&failure);
    require(failure.message && value==original,"unrooted no-match input accepted");
    failure={};auto bad=element;bad.bytes=1;
    vd_text_translate(&value,&value,&keys,&replacements,&bad,16,&failure);
    require(failure.message && value==original,"malformed replacement schema accepted");
    failure={};
    vd_text_translate(&value,&value,&replacements,&keys,&element,16,&failure);
    require(failure.message && value==original,"wrong list schemas accepted");
    // Same table identities and backings, new key/replacement contents.
    runtime.list_write(runtime.root_word(&keys),vd::ScalarEncoding::i32,0,'o');failure={};
    require(invoke(&output,16,failure)==0 && !failure.message && runtime.string_bytes(output)=="Rrdinary message","mutated matching key was cached");
    runtime.string_into(&replacement,"QQ");runtime.array_write(runtime.root_word(&replacements),layout,true,0,&replacement);
    require(invoke(&output,17,failure)==0 && !failure.message && runtime.string_bytes(output)=="QQrdinary message","mutated replacement was cached");
    runtime.list_write(runtime.root_word(&keys),vd::ScalarEncoding::i32,0,1000);
    // Deliberately corrupt a separately rooted replacement extent, restore it
    // before collection, and verify Runtime failure/error precedence.
    runtime.string_into(&replacement,"late");
    runtime.array_write(runtime.root_word(&replacements),layout,true,80,&replacement);
    std::uint64_t valid_extent; std::memcpy(&valid_extent,replacement,sizeof(valid_extent));
    auto corrupt_extent=valid_extent+1;std::memcpy(replacement,&corrupt_extent,sizeof(corrupt_extent));
    auto before_output=output;failure={};
    auto corrupt_status=invoke(&output,16,failure);
    std::memcpy(replacement,&valid_extent,sizeof(valid_extent));
    require(failure.message && output==before_output,"corrupt replacement extent accepted or published");
    runtime.list_write(runtime.root_word(&keys),vd::ScalarEncoding::i32,0,0xffffffffu);
    std::memcpy(replacement,&corrupt_extent,sizeof(corrupt_extent));failure={};
    corrupt_status=invoke(&output,-1,failure);
    std::memcpy(replacement,&valid_extent,sizeof(valid_extent));
    require(!failure.message && corrupt_status==2 && output==before_output,"later replacement masked invalid first key");
    runtime.list_write(runtime.root_word(&keys),vd::ScalarEncoding::i32,0,1000);
    runtime.list_write(runtime.root_word(&keys),vd::ScalarEncoding::i32,1,1000);
    std::memcpy(replacement,&corrupt_extent,sizeof(corrupt_extent));failure={};
    corrupt_status=invoke(&output,-1,failure);
    std::memcpy(replacement,&valid_extent,sizeof(valid_extent));
    require(!failure.message && corrupt_status==2 && output==before_output,"later replacement masked duplicate key");
    runtime.list_write(runtime.root_word(&keys),vd::ScalarEncoding::i32,1,1001);
    failure={};
    // Pending collection must complete across the no-match operation boundary.
    vd::Runtime::Collection collected{};
    std::thread collector([&]{vd::Runtime::Attachment other(runtime);vd::Runtime::Managed collecting(runtime);collected=runtime.collect();});
    while(!runtime.snapshot().requested)std::this_thread::yield();
    require(invoke(&value,16,failure)==0 && !failure.message && value==original,"pending-GC no-match alias failed");
    runtime.poll();collector.join();
    require(collected==vd::Runtime::Collection::completed,"no-match blocked collector");
    require(runtime.collect()==vd::Runtime::Collection::completed && runtime.string_bytes(value)=="ordinary message","no-match alias not rooted after GC");
    }
    std::cout<<checks<<" bounded no-match native checks passed\n";
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
