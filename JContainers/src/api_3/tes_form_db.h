#include "boost_extras.h"

namespace tes_api_3 {

    using namespace collections;

    class tes_form_db : public class_meta<tes_form_db> {
    public:

        REGISTER_TES_NAME("JFormDB");

        void additionalSetup() {
            metaInfo.comment = "Manages form related information (entry).\n";
        }

        enum path_type {
            is_key = 0,
            is_path = 1,
        };

        class subpath_extractor {
            enum { bytesCount = 0xff };

            const char *_rest;
            char _storageName[bytesCount];
        public:

            explicit subpath_extractor(const char * const path, path_type type = is_key) {
                _storageName[0] = '\0';
                _rest = nullptr;

                if (!path) {
                    return;
                }

                namespace bs = boost;

                auto pathRange = bs::make_iterator_range(path, path + strlen(path));

                auto pair1 = bs::half_split(pathRange, ".");

                if (!pair1.first.empty() || pair1.second.empty()) {
                    return;
                }

                auto pair2 = bs::half_split_if(pair1.second, bs::is_any_of(".["));

                if (!pair2.first.empty() && !pair2.second.empty()) {
                    auto strorageNameLen = std::min<size_t>(bytesCount-1,  pair2.first.size());
                    std::copy_n(pair2.first.begin(), strorageNameLen, _storageName);
                    _storageName[strorageNameLen] = '\0';

                    _rest = (type == is_key ?
                        pair2.second.begin() : pair2.second.begin() - 1);
                }
            }

            const char *rest() const {
                return _rest;
            }

            const char *storageName() const {
                return _rest ? _storageName : nullptr;
            }
        };

        using key_cref = const form_ref_lightweight&;

        static form_map *makeFormStorage(tes_context& ctx, const char *storageName) {
            if (!validate_storage_name(storageName)) {
                return nullptr;
            }

            auto& db = ctx.root();
            form_map::ref fmap = tes_map::getItem<object_base*>(ctx, &db, storageName)->as<form_map>();

            if (!fmap) {
                fmap = tes_object::object<form_map>(ctx);
                tes_db::setObj(ctx, storageName, fmap.to_base<object_base>());
            }

            return fmap.get();
        }

        static bool validate_storage_name(const char *name) {
            return name && *name;
        }

        static void setEntry(tes_context& ctx, const char *storageName, key_cref formKey, object_stack_ref& entry) {
            if (!validate_storage_name(storageName) || !formKey) {
                return;
            }

            if (entry) {
                auto fmap = makeFormStorage(ctx, storageName);
                tes_form_map::setItem(ctx, fmap, formKey, entry);
            } else {
                auto& db = ctx.root();
                auto fmap = tes_map::getItem<object_base*>(ctx, &db, storageName)->as<form_map>();
                tes_form_map::removeKey(ctx, fmap, formKey);
            }
        }
        REGISTERF2(setEntry, "storageName fKey entry", "associates given form key and entry (container). set entry to zero to destroy association");

        static map *makeMapEntry(tes_context& ctx, const char *storageName, key_cref form) {
            if (!form || !validate_storage_name(storageName)) {
                return nullptr;
            }

            form_map *fmap = makeFormStorage(ctx, storageName);
            map *entry = tes_form_map::getItem<object_base*>(ctx, fmap, form)->as<map>();
            if (!entry) {
                entry = tes_object::object<map>(ctx);
                tes_form_map::setItem(ctx, fmap, form, entry);
            }

            return entry;
        }
        REGISTERF(makeMapEntry, "makeEntry", "storageName fKey", "returns (or creates new if not found) JMap entry for given storage and form");

        static object_base *findEntry(tes_context& ctx, const char *storageName, key_cref form) {
            auto& db = ctx.root();
            form_map *fmap = tes_map::getItem<object_base*>(ctx, &db, storageName)->as<form_map>();
            return tes_form_map::getItem<object_base*>(ctx, fmap, form);
        }
        REGISTERF2(findEntry, "storageName fKey", "search for entry for given storage and form");

        static map *findMapEntry(tes_context& ctx, const char *storageName, key_cref form) {
            return findEntry(ctx, storageName, form)->as<map>();
        }

        //////////////////////////////////////////////////////////////////////////

        template<class T>
        static T solveGetter(tes_context& ctx, key_cref form, const char* path, T t= default_value<T>()) {
            subpath_extractor sub(path, is_path);
            return tes_object::resolveGetter<T>(ctx, findEntry(ctx, sub.storageName(), form), sub.rest(), t);
        }
        REGISTERF(solveGetter<Float32>, "solveFlt", "fKey path default=0.0", "attempts to get value associated with path.");
        REGISTERF(solveGetter<SInt32>, "solveInt", "fKey path default=0", nullptr);
        REGISTERF(solveGetter<skse::string_ref>, "solveStr", "fKey path default=\"\"", nullptr);
        REGISTERF(solveGetter<Handle>, "solveObj", "fKey path default=0", nullptr);
        REGISTERF(solveGetter<form_ref>, "solveForm", "fKey path default=None", nullptr);

        template<class T>
        static bool solveSetter(tes_context& ctx, key_cref form, const char* path, T value, bool createMissingKeys = false) {
            subpath_extractor sub(path, is_path);
            return tes_object::solveSetter(ctx, findEntry(ctx, sub.storageName(), form), sub.rest(), value, createMissingKeys);
        }
        REGISTERF(solveSetter<Float32>, "solveFltSetter", "fKey path value createMissingKeys=false",
            "Attempts to assign value. Returns false if no such path\n"
            "With 'createMissingKeys=true' it creates any missing path elements: JFormDB.solveIntSetter(formKey, \".frostfall.keyB\", 10, true) creates {frostfall: {keyB: 10}} structure");
        REGISTERF(solveSetter<SInt32>, "solveIntSetter", "fKey path value createMissingKeys=false", nullptr);
        REGISTERF(solveSetter<const char*>, "solveStrSetter", "fKey path value createMissingKeys=false", nullptr);
        REGISTERF(solveSetter<object_stack_ref&>, "solveObjSetter", "fKey path value createMissingKeys=false", nullptr);
        REGISTERF(solveSetter<form_ref>, "solveFormSetter", "fKey path value createMissingKeys=false", nullptr);

        static bool hasPath(tes_context& ctx, key_cref form, const char* path) {
            subpath_extractor sub(path);
            return tes_object::hasPath(ctx, findMapEntry(ctx, sub.storageName(), form), sub.rest());
        }
        REGISTERF2(hasPath, "fKey path", "returns true, if capable resolve given path, e.g. it able to execute solve* or solver*Setter functions successfully");

        //////////////////////////////////////////////////////////////////////////

        static object_base* allKeys(tes_context& ctx, key_cref form, const char *path) {
            subpath_extractor sub(path);
            return tes_map::allKeys(ctx, findMapEntry(ctx, sub.storageName(), form));
        }
        REGISTERF2(allKeys, "fKey key",
            "JMap-like interface functions:\n"
            "\n"
            "returns new array containing all keys");

        static object_base* allValues(tes_context& ctx, key_cref form, const char *path) {
            subpath_extractor sub(path);
            return tes_map::allValues(ctx, findMapEntry(ctx, sub.storageName(), form));
        }
        REGISTERF2(allValues, "fKey key", "returns new array containing all values");

        template<class T>
        static T getItem(tes_context& ctx, key_cref form, const char* path) {
            subpath_extractor sub(path);
            return tes_map::getItem<T>(ctx, findMapEntry(ctx, sub.storageName(), form), sub.rest());
        }
        // TODO: where is default value parameter?
        REGISTERF(getItem<SInt32>, "getInt", "fKey key", "returns value associated with key");
        REGISTERF(getItem<Float32>, "getFlt", "fKey key", "");
        REGISTERF(getItem<skse::string_ref>, "getStr", "fKey key", "");
        REGISTERF(getItem<object_base *>, "getObj", "fKey key", "");
        REGISTERF(getItem<form_ref>, "getForm", "fKey key", "");

        template<class T>
        static void setItem(tes_context& ctx, key_cref form, const char* path, T item) {
            subpath_extractor sub(path);
            tes_map::setItem(ctx, makeMapEntry(ctx, sub.storageName(), form), sub.rest(), item);
        }
        REGISTERF(setItem<SInt32>, "setInt", "fKey key value", "creates key-value association. replaces existing value if any");
        REGISTERF(setItem<Float32>, "setFlt", "fKey key value", "");
        REGISTERF(setItem<const char *>, "setStr", "fKey key value", "");
        REGISTERF(setItem<object_stack_ref&>, "setObj", "fKey key container", "");
        REGISTERF(setItem<form_ref>, "setForm", "fKey key value", "");
    };

    TES_META_INFO(tes_form_db);

    TEST(tes_form_db, subpath_extractor)
    {
        auto expectKeyEq = [&](const char *path, const char *storageName, const char *asKey, const char *asPath) {
            auto extr = tes_form_db::subpath_extractor(path);
            EXPECT_TRUE((!storageName && !extr.storageName()) || strcmp(extr.storageName(), storageName) == 0 );
            EXPECT_TRUE((!asKey && !extr.rest()) || strcmp(extr.rest(), asKey) == 0 );

            auto pathExtr = tes_form_db::subpath_extractor(path, tes_form_db::is_path);
            EXPECT_TRUE((!storageName && !pathExtr.storageName()) || strcmp(pathExtr.storageName(), storageName) == 0 );
            EXPECT_TRUE((!asPath && !pathExtr.rest()) || strcmp(pathExtr.rest(), asPath) == 0 );
        };

        auto expectFail = [&](const char *path) {
            expectKeyEq(path, nullptr, nullptr, nullptr);
        };

        expectKeyEq(".strg.key", "strg", "key", ".key");
        expectKeyEq(".strg[0]", "strg", "0]", "[0]");
        expectKeyEq(".strg[0].key.key2", "strg", "0].key.key2", "[0].key.key2");

        expectFail("strg[0]");
        expectFail("[");
        expectFail(nullptr);
        expectFail("");
        expectFail("...");
    }

    TEST(tes_form_db, storage_and_entry)
    {
        tes_context_standalone ctx;

        const char *storageName = "forms";

        auto formStorage = tes_form_db::makeFormStorage(ctx, storageName);
        EXPECT_NOT_NIL(formStorage);
        EXPECT_EQ(formStorage, tes_form_db::makeFormStorage(ctx, storageName));

        auto fakeForm = make_lightweight_form_ref((FormId)0x14, ctx);

        auto entry = tes_form_db::makeMapEntry(ctx, storageName, fakeForm);
        EXPECT_NOT_NIL(entry);
        EXPECT_EQ(entry, tes_form_db::makeMapEntry(ctx, storageName, fakeForm));
    }


    TEST(tes_form_db, get_set)
    {
        tes_context_standalone ctx;

        auto fakeForm = make_lightweight_form_ref((FormId)0x14, ctx);

        const char *path = ".forms.object";

        auto ar = tes_array::objectWithSize(ctx, 0);
        EXPECT_NOT_NIL(ar);
        tes_form_db::setItem(ctx, fakeForm, path, ar);

        EXPECT_TRUE(ar == tes_form_db::getItem<object_base*>(ctx, fakeForm, path));
        EXPECT_TRUE(ar == tes_form_db::solveGetter<object_base*>(ctx, fakeForm, path));
    }


}
