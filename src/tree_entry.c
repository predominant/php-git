/*
 * The MIT License
 *
 * Copyright (c) 2010 - 2011 Shuhei Tanuma
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "php_git.h"
#include <spl/spl_array.h>
#include <zend_interfaces.h>
#include <string.h>
#include <time.h>

PHPAPI zend_class_entry *git_tree_entry_class_entry;

ZEND_BEGIN_ARG_INFO_EX(arginfo_git_tree_entry_set_id, 0, 0, 1)
    ZEND_ARG_INFO(0, hash)
ZEND_END_ARG_INFO()

static void php_git_tree_entry_free_storage(php_git_tree_entry_t *obj TSRMLS_DC)
{
    zend_object_std_dtor(&obj->zo TSRMLS_CC);
    
    obj->entry = NULL;
    efree(obj);
}

zend_object_value php_git_tree_entry_new(zend_class_entry *ce TSRMLS_DC)
    {
    zend_object_value retval;
    php_git_tree_entry_t *obj;
    zval *tmp;

    obj = ecalloc(1, sizeof(*obj));
    zend_object_std_init( &obj->zo, ce TSRMLS_CC );
    zend_hash_copy(obj->zo.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));

    retval.handle = zend_objects_store_put(obj, 
        (zend_objects_store_dtor_t)zend_objects_destroy_object,
        (zend_objects_free_object_storage_t)php_git_tree_entry_free_storage,
        NULL TSRMLS_CC);
    retval.handlers = zend_get_std_object_handlers();
    return retval;
}


PHP_METHOD(git_tree_entry, toObject)
{
    php_git_tree_entry_t *this;
    git_object *object;
    git_otype type;
    zval *git_raw_object, *git_tree;
    php_git_tree_t *tobj;
    int ret;
    
    this = (php_git_tree_entry_t *) zend_object_store_get_object(getThis() TSRMLS_CC);

    ret = git_tree_entry_2object(&object, this->repository, this->entry);

    if(ret == GIT_SUCCESS){
        type = git_object_type(object);
        if (type == GIT_OBJ_BLOB) {
            MAKE_STD_ZVAL(git_raw_object);
            object_init_ex(git_raw_object, git_blob_class_entry);
            php_git_blob_t *blobobj = (php_git_blob_t *) zend_object_store_get_object(git_raw_object TSRMLS_CC);
            blobobj->object = (git_blob *)object;

            add_property_stringl_ex(git_raw_object,"data", sizeof("data"), (char *)git_blob_rawcontent((git_blob *)object),git_blob_rawsize((git_blob *)object), 1 TSRMLS_CC);
            RETURN_ZVAL(git_raw_object,0,1);
        } else if(type == GIT_OBJ_TREE) {

            MAKE_STD_ZVAL(git_tree);
            object_init_ex(git_tree, git_tree_class_entry);

            tobj = (php_git_tree_t *) zend_object_store_get_object(git_tree TSRMLS_CC);
            tobj->object = object;
            tobj->repository = this->repository;

            RETURN_ZVAL(git_tree,0,1);

        } else {
            zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC,
                "Git\\Tree\\Entry::toObject can convert GIT_OBJ_TREE or GIT_OBJ_BLOB. unhandled object type %d found.", git_object_type(object));
            RETURN_FALSE;
        }
    }

}

PHP_METHOD(git_tree_entry, __construct)
{
}

PHP_METHOD(git_tree_entry, isTree)
{
    php_git_tree_entry_t *this = (php_git_tree_entry_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
    int attribute = git_tree_entry_attributes(this->entry);

    if(attribute & 040000){
        RETURN_TRUE;
    }else{
        RETURN_FALSE;
    }
}

PHP_METHOD(git_tree_entry, isBlob)
{
    php_git_tree_entry_t *this = (php_git_tree_entry_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
    int attribute = git_tree_entry_attributes(this->entry);
    if(!(attribute & 040000)){
        RETURN_TRUE;
    }else{
        RETURN_FALSE;
    }
}

PHP_METHOD(git_tree_entry, toHeader)
{
    php_git_tree_entry_t *this = (php_git_tree_entry_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
    git_odb *odb;
    git_oid oid;
    git_otype type;
    zval *git_raw_object;
    int length;
    char *id;

    id = Z_STRVAL_P(zend_read_property(git_tree_entry_class_entry, getThis(),"oid",sizeof("oid")-1, 0 TSRMLS_CC));

    git_oid_fromstr(&oid, id);
    git_odb_read_header(&length,&type,git_repository_database(this->repository),&oid);

    if (type == GIT_OBJ_BLOB) {
        MAKE_STD_ZVAL(git_raw_object);
        object_init_ex(git_raw_object, git_blob_class_entry);

        add_property_string_ex(git_raw_object,"data", sizeof("data"), "", 1 TSRMLS_CC);
        add_property_long_ex(git_raw_object,"size", sizeof("size"), length TSRMLS_CC);
        RETURN_ZVAL(git_raw_object,0,1);
    } else {
        zend_throw_exception_ex(spl_ce_RuntimeException, 0 TSRMLS_CC,"Git\\Tree\\Entry::toHeader can convert GIT_OBJ_BLOB only for now. unhandled object type %d found.", type);
    }
}

static zend_function_entry php_git_tree_entry_methods[] = {
    PHP_ME(git_tree_entry, __construct, NULL, ZEND_ACC_PRIVATE)
    PHP_ME(git_tree_entry, toHeader, NULL,    ZEND_ACC_PUBLIC)
    PHP_ME(git_tree_entry, toObject, NULL,    ZEND_ACC_PUBLIC)
    PHP_ME(git_tree_entry, isTree,   NULL,    ZEND_ACC_PUBLIC)
    PHP_ME(git_tree_entry, isBlob,   NULL,    ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

void git_init_tree_entry(TSRMLS_D)
{
    zend_class_entry git_tree_entry_ce;
    INIT_NS_CLASS_ENTRY(git_tree_entry_ce, ZEND_NS_NAME(PHP_GIT_NS,"Tree"),"Entry", php_git_tree_entry_methods);
    git_tree_entry_class_entry = zend_register_internal_class(&git_tree_entry_ce TSRMLS_CC);
    git_tree_entry_class_entry->create_object = php_git_tree_entry_new;

    zend_declare_property_null(git_tree_entry_class_entry, "name",sizeof("name")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(git_tree_entry_class_entry, "oid",sizeof("oid")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(git_tree_entry_class_entry, "mode",sizeof("mode")-1, ZEND_ACC_PUBLIC TSRMLS_CC);

}
