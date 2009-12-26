#include "../pointless.h"

static void PyPointlessVector_dealloc(PyPointlessVector* self)
{
	Py_XDECREF(self->pp);
	self->pp = 0;
	self->v = 0;
	self->container_id = 0;
	self->is_hashable = 0;
	self->slice_i = 0;
	self->slice_n = 0;
	self->ob_type->tp_free((PyObject*)self);
}

static void PyPointlessVectorIter_dealloc(PyPointlessVectorIter* self)
{
	Py_XDECREF(self->vector);
	self->vector = 0;
	self->iter_state = 0;
	self->ob_type->tp_free((PyObject*)self);
}

PyObject* PyPointlessVector_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
	PyPointlessVector* self = (PyPointlessVector*)type->tp_alloc(type, 0);

	if (self) {
		self->pp = 0;
		self->v = 0;
		self->container_id = 0;
		self->is_hashable = 0;
		self->slice_i = 0;
		self->slice_n = 0;
	}

	return (PyObject*)self;
}

PyObject* PyPointlessVectorIter_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
	PyPointlessVectorIter* self = (PyPointlessVectorIter*)type->tp_alloc(type, 0);

	if (self) {
		self->vector = 0;
		self->iter_state = 0;
	}

	return (PyObject*)self;
}

static int PyPointlessVector_init(PyPointlessVector* self, PyObject* args)
{
	PyErr_SetString(PyExc_TypeError, "unable to instantiate PyPointlessVector directly");
	return -1;
}

static int PyPointlessVectorIter_init(PyPointlessVector* self, PyObject* args)
{
	PyErr_SetString(PyExc_TypeError, "unable to instantiate PyPointlessVectorIter directly");
	return -1;
}

static Py_ssize_t PyPointlessVector_length(PyPointlessVector* self)
{
	return (Py_ssize_t)self->slice_n;
}

static int PyPointlessVector_check_index(PyPointlessVector* self, PyObject* item, Py_ssize_t* i)
{
	// if this is not an index: throw an exception
	if (!PyIndex_Check(item)) {
		PyErr_Format(PyExc_TypeError, "PointlessVector: integer indexes please, got <%s>\n", item->ob_type->tp_name);
		return 0;
	}

	// if index value is not an integer: throw an exception
	*i = PyNumber_AsSsize_t(item, PyExc_IndexError);

	if (*i == -1 && PyErr_Occurred())
		return 0;

	// if index is negative, it is relative to vector end
	if (*i < 0)
		*i += PyPointlessVector_length(self);

	// if it is out of bounds: throw an exception
	if (!(0 <= *i && *i < PyPointlessVector_length(self))) {
		PyErr_SetString(PyExc_IndexError, "index is out of bounds");
		return 0;
	}

	return 1;
}

static PyObject* PyPointlessVector_subscript_priv(PyPointlessVector* self, uint32_t i)
{
	i += self->slice_i;

	switch (self->v->type) {
		case POINTLESS_VECTOR_VALUE:
		case POINTLESS_VECTOR_VALUE_HASHABLE:
			return pypointless_value(self->pp, pointless_reader_vector_value(&self->pp->p, self->v) + i);
		case POINTLESS_VECTOR_I8:
			return pypointless_i32(self->pp, (int32_t)pointless_reader_vector_i8(&self->pp->p, self->v)[i]);
		case POINTLESS_VECTOR_U8:
			return pypointless_u32(self->pp, (uint32_t)pointless_reader_vector_u8(&self->pp->p, self->v)[i]);
		case POINTLESS_VECTOR_I16:
			return pypointless_i32(self->pp, (int32_t)pointless_reader_vector_i16(&self->pp->p, self->v)[i]);
		case POINTLESS_VECTOR_U16:
			return pypointless_u32(self->pp, (uint32_t)pointless_reader_vector_u16(&self->pp->p, self->v)[i]);
		case POINTLESS_VECTOR_I32:
			return pypointless_i32(self->pp, pointless_reader_vector_i32(&self->pp->p, self->v)[i]);
		case POINTLESS_VECTOR_U32:
			return pypointless_u32(self->pp, pointless_reader_vector_u32(&self->pp->p, self->v)[i]);
		case POINTLESS_VECTOR_FLOAT:
			return pypointless_float(self->pp, pointless_reader_vector_float(&self->pp->p, self->v)[i]);
	}

	PyErr_Format(PyExc_TypeError, "strange array type");
	return 0;
}

static PyObject* PyPointlessVector_subscript(PyPointlessVector* self, PyObject* item)
{
	// get the index
	Py_ssize_t i;

	if (!PyPointlessVector_check_index(self, item, &i))
		return 0;

	return PyPointlessVector_subscript_priv(self, (uint32_t)i);
}

static PyObject* PyPointlessVector_item(PyPointlessVector* self, Py_ssize_t i)
{
	if (!(0 <= i && i < self->slice_n)) {
		PyErr_SetString(PyExc_IndexError, "vector index out of range");
		return 0;
	}

	return PyPointlessVector_subscript_priv(self, (uint32_t)i);
}

static int PyPointlessVector_contains(PyPointlessVector* self, PyObject* b)
{
	uint32_t i, c;
	pointless_value_t v;
	const char* error = 0;

	for (i = 0; i < self->slice_n; i++) {
		v = pointless_reader_vector_value_case(&self->pp->p, self->v, i + self->slice_i);
		c = pypointless_cmp_eq(&self->pp->p, &v, b, &error);

		if (error) {
			PyErr_Format(PyExc_ValueError, "comparison error: %s", error);
			return -1;
		}

		if (c)
			return 1;
	}

	return 0;
}

static PyObject* PyPointlessVector_slice(PyPointlessVector* self, Py_ssize_t ilow, Py_ssize_t ihigh)
{
	// clamp the limits
	uint32_t n_items = self->slice_n;

	if (ilow < 0)
		ilow = 0;
	else if (ilow > (Py_ssize_t)n_items)
		ilow = (Py_ssize_t)n_items;

	if (ihigh < ilow)
		ihigh = ilow;
	else if (ihigh > n_items)
		ihigh = (Py_ssize_t)n_items;

	uint32_t slice_i = self->slice_i + (uint32_t)ilow;
	uint32_t slice_n = (uint32_t)(ihigh - ilow);

	return (PyObject*)PyPointlessVector_New(self->pp, self->v, slice_i, slice_n);
}

static PyObject* PyPointlessVector_iter(PyObject* vector)
{
	if (!PyPointlessVector_Check(vector)) {
		PyErr_BadInternalCall();
		return 0;
	}

	PyPointlessVectorIter* iter = PyObject_New(PyPointlessVectorIter, &PyPointlessVectorIterType);

	if (iter == 0)
		return 0;

	iter = (PyPointlessVectorIter*)PyObject_Init((PyObject*)iter, &PyPointlessVectorIterType);

	Py_INCREF(vector);

	iter->vector = (PyPointlessVector*)vector;
	iter->iter_state = 0;

	return (PyObject*)iter;
}

static PyObject* PyPointlessVectorIter_iternext(PyPointlessVectorIter* iter)
{
	// iterator already reached end
	if (iter->vector == 0)
		return 0;

	// see if we have any items left
	uint32_t n_items = (uint32_t)PyPointlessVector_length(iter->vector);

	if (iter->iter_state < n_items) {
		PyObject* item = PyPointlessVector_subscript_priv(iter->vector, iter->iter_state);
		iter->iter_state += 1;
		return item;
	}

	// we reached end
	Py_DECREF(iter->vector);
	iter->vector = 0;
	return 0;
}

static PyObject* PyPointlessVector_richcompare(PyObject* a, PyObject* b, int op)
{
	if (!PyPointlessVector_Check(a) || !PyPointlessVector_Check(b)) {
		Py_INCREF(Py_NotImplemented);
		return Py_NotImplemented;
	}

	uint32_t n_items_a = (uint32_t)PyPointlessVector_length((PyPointlessVector*)a);
	uint32_t n_items_b = (uint32_t)PyPointlessVector_length((PyPointlessVector*)b);
	uint32_t i, n_items = (n_items_a < n_items_b) ? n_items_a : n_items_b;
	int32_t c;

	if (n_items_a != n_items_b && (op == Py_EQ || op == Py_NE)) {
		if (op == Py_EQ)
			Py_RETURN_FALSE;
		else
			Py_RETURN_TRUE;
	}

	// first item where they differ
	for (i = 0; i < n_items; i++) {
		PyObject* obj_a = PyPointlessVector_subscript_priv((PyPointlessVector*)a, i);
		PyObject* obj_b = PyPointlessVector_subscript_priv((PyPointlessVector*)b, i);

		if (obj_a == 0 || obj_b == 0) {
			Py_XDECREF(obj_a);
			Py_XDECREF(obj_b);
			return 0;
		}

		int k = PyObject_RichCompareBool(obj_a, obj_b, Py_EQ);

		Py_DECREF(obj_a);
		Py_DECREF(obj_b);

		obj_a = obj_b = 0;

		if (k < 0)
			return 0;

		if (k == 0)
			break;
	}

	// if all items up to n_items are the same, just compare on lengths
	if (i >= n_items_a || i >= n_items_b) {
		switch (op) {
			case Py_LT: c = (n_items_a <  n_items_b); break;
			case Py_LE: c = (n_items_a <= n_items_b); break;
			case Py_EQ: c = (n_items_a == n_items_b); break;
			case Py_NE: c = (n_items_a != n_items_b); break;
			case Py_GT: c = (n_items_a >  n_items_b); break;
			case Py_GE: c = (n_items_a >= n_items_b); break;
			default: assert(0); return 0;
		}

		if (c)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}

	// items are different
	if (op == Py_EQ)
		Py_RETURN_FALSE;
	else if (op == Py_NE)
		Py_RETURN_TRUE;

	// dig a bit deeper
	PyObject* obj_a = PyPointlessVector_subscript_priv((PyPointlessVector*)a, i);
	PyObject* obj_b = PyPointlessVector_subscript_priv((PyPointlessVector*)b, i);

	if (obj_a == 0 || obj_b == 0) {
		Py_XDECREF(obj_a);
		Py_XDECREF(obj_b);
		return 0;
	}

	PyObject* comp = PyObject_RichCompare(obj_a, obj_b, op);

	Py_DECREF(obj_a);
	Py_DECREF(obj_b);

	return comp;
}

static PyObject* PyPointlessVector_get_typecode(PyPointlessVector* a, void* closure)
{
	const char* s = 0;
	const char* e = 0;

	switch (a->v->type) {
		case POINTLESS_VECTOR_VALUE:
		case POINTLESS_VECTOR_VALUE_HASHABLE:
			e = "this is a value-based vector";
			break;
		case POINTLESS_VECTOR_EMPTY:
			e = "empty vectors are typeless";
			break;
		case POINTLESS_VECTOR_I8:    s = "i8"; break;
		case POINTLESS_VECTOR_U8:    s = "u8"; break;
		case POINTLESS_VECTOR_I16:   s = "i16"; break;
		case POINTLESS_VECTOR_U16:   s = "u16"; break;
		case POINTLESS_VECTOR_I32:   s = "i32"; break;
		case POINTLESS_VECTOR_U32:   s = "u32"; break;
		case POINTLESS_VECTOR_FLOAT: s = "f"; break;
		default:
			PyErr_BadInternalCall();
			return 0;
	}

	if (e) {
		PyErr_SetString(PyExc_ValueError, e);
		return 0;
	}

	return Py_BuildValue("s", s);
}

static PyGetSetDef PyPointlessVector_getsets [] = {
	{"typecode", (getter)PyPointlessVector_get_typecode, 0, "the typecode character used to create the vector"},
	{NULL}
};

static PyMemberDef PyPointlessVector_memberlist[] = {
        {"container_id",  T_ULONG, offsetof(PyPointlessVector, container_id), READONLY},
		{"is_hashable", T_INT, offsetof(PyPointlessVector, is_hashable), READONLY},
		{NULL}
};

static PyMappingMethods PyPointlessVector_as_mapping = {
	(lenfunc)PyPointlessVector_length,
	(binaryfunc)PyPointlessVector_subscript,
	(objobjargproc)0
};

static PySequenceMethods PyPointlessVector_as_sequence = {
	(lenfunc)PyPointlessVector_length,          /* sq_length */
	0,                                          /* sq_concat */
	0,                                          /* sq_repeat */
	(ssizeargfunc)PyPointlessVector_item,       /* sq_item */
	(ssizessizeargfunc)PyPointlessVector_slice, /* sq_slice */
	0,                                          /* sq_ass_item */
	0,                                          /* sq_ass_slice */
	(objobjproc)PyPointlessVector_contains,     /* sq_contains */
	0,                                          /* sq_inplace_concat */
	0,                                          /* sq_inplace_repeat */
};

PyTypeObject PyPointlessVectorType = {
	PyObject_HEAD_INIT(NULL)
	0,                                     /*ob_size*/
	"pointless.PyPointlessVector",         /*tp_name*/
	sizeof(PyPointlessVector),             /*tp_basicsize*/
	0,                                     /*tp_itemsize*/
	(destructor)PyPointlessVector_dealloc, /*tp_dealloc*/
	0,                                     /*tp_print*/
	0,                                     /*tp_getattr*/
	0,                                     /*tp_setattr*/
	0,                                     /*tp_compare*/
	PyPointless_repr,                      /*tp_repr*/
	0,                                     /*tp_as_number*/
	&PyPointlessVector_as_sequence,        /*tp_as_sequence*/
	&PyPointlessVector_as_mapping,         /*tp_as_mapping*/
	0,                                     /*tp_hash */
	0,                                     /*tp_call*/
	PyPointless_str,                       /*tp_str*/
	0,                                     /*tp_getattro*/
	0,                                     /*tp_setattro*/
	0,                                     /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,                    /*tp_flags*/
	"PyPointlessVector wrapper",           /*tp_doc */
	0,                                     /*tp_traverse */
	0,                                     /*tp_clear */
	PyPointlessVector_richcompare,         /*tp_richcompare */
	0,                                     /*tp_weaklistoffset */
	PyPointlessVector_iter,                /*tp_iter */
	0,                                     /*tp_iternext */
	0,                                     /*tp_methods */
	PyPointlessVector_memberlist,          /*tp_members */
	PyPointlessVector_getsets,             /*tp_getset */
	0,                                     /*tp_base */
	0,                                     /*tp_dict */
	0,                                     /*tp_descr_get */
	0,                                     /*tp_descr_set */
	0,                                     /*tp_dictoffset */
	(initproc)PyPointlessVector_init,      /*tp_init */
	0,                                     /*tp_alloc */
	PyPointlessVector_new,                 /*tp_new */
};

PyTypeObject PyPointlessVectorIterType = {
	PyObject_HEAD_INIT(NULL)
	0,                                            /*ob_size*/
	"pointless.PyPointlessVectorIter",                /*tp_name*/
	sizeof(PyPointlessVectorIter),                /*tp_basicsize*/
	0,                                            /*tp_itemsize*/
	(destructor)PyPointlessVectorIter_dealloc,    /*tp_dealloc*/
	0,                                            /*tp_print*/
	0,                                            /*tp_getattr*/
	0,                                            /*tp_setattr*/
	0,                                            /*tp_compare*/
	0,                                            /*tp_repr*/
	0,                                            /*tp_as_number*/
	0,                                            /*tp_as_sequence*/
	0,                                            /*tp_as_mapping*/
	0,                                            /*tp_hash */
	0,                                            /*tp_call*/
	0,                                            /*tp_str*/
	0,                                            /*tp_getattro*/
	0,                                            /*tp_setattro*/
	0,                                            /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,                           /*tp_flags*/
	"PyPointlessVectorIter",                      /*tp_doc */
	0,                                            /*tp_traverse */
	0,                                            /*tp_clear */
	0,                                            /*tp_richcompare */
	0,                                            /*tp_weaklistoffset */
	PyObject_SelfIter,                            /*tp_iter */
	(iternextfunc)PyPointlessVectorIter_iternext, /*tp_iternext */
	0,                                            /*tp_methods */
	0,                                            /*tp_members */
	0,                                            /*tp_getset */
	0,                                            /*tp_base */
	0,                                            /*tp_dict */
	0,                                            /*tp_descr_get */
	0,                                            /*tp_descr_set */
	0,                                            /*tp_dictoffset */
	(initproc)PyPointlessVectorIter_init,         /*tp_init */
	0,                                            /*tp_alloc */
	PyPointlessVectorIter_new,                    /*tp_new */
};

PyPointlessVector* PyPointlessVector_New(PyPointless* pp, pointless_value_t* v, uint32_t slice_i, uint32_t slice_n)
{
	if (!(slice_i + slice_n <= pointless_reader_vector_n_items(&pp->p, v))) {
		PyErr_SetString(PyExc_ValueError, "slice invariant error when creating PyPointlessVector");
		return 0;
	}

	PyPointlessVector* pv = PyObject_New(PyPointlessVector, &PyPointlessVectorType);

	if (pv == 0)
		return 0;

	pv = (PyPointlessVector*)PyObject_Init((PyObject*)pv, &PyPointlessVectorType);

	Py_INCREF(pp);

	pv->pp = pp;
	pv->v = v;

	// the container ID is a unique between all non-empty vectors, maps and sets
	pv->container_id = (unsigned long)pointless_container_id(&pp->p, v);
	pv->is_hashable = (int)pointless_is_hashable(v->type);

	pv->slice_i = slice_i;
	pv->slice_n = slice_n;

	return pv;
}
